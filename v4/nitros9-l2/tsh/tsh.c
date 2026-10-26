// tsh.c -- a shell with line editing, history, completion and a command
// search path, for NitrOS-9 Level 2 on the TFR/911H.
//
//   tsh [<file>]
//
// tsh reads each command line itself, then gives it to the OS-9 Shell to
// run ("Shell <line>"), so commands keep the Shell's syntax: ! pipes,
// < > >> >>> <> <>> <>>> redirection, #<mem>, & and ;, ( ) and
// procedure files.  What tsh adds:
//
//   Editing   Left Right ^B ^F, ^A ^E (start, end), Esc-b Esc-f (words),
//             Backspace, ^D (delete), ^W (word before), Esc-d, Esc-Backspace,
//             ^K ^U (to the end, to the start), ^X (the line), ^Y (put back),
//             ^T (swap), ^L (redraw), ^C (cancel), Tab (complete).
//   History   Up Down ^P ^N; Shift-Up Shift-Down (lines that start with what
//             is before the cursor); ^R (search).  Kept in /DD/SYS/tsh_history,
//             ten lines at a time and on exit.
//   Path      A command that is neither in memory nor in the execution
//             directory is looked for in each directory of "path".
//   Built in  cd chd chx path history alias unalias source . which set
//             w wait kill setpr x -x p -p t -t ex exit, * comments, and the
//             settings PATH=<dirs> PROMPT=<text> COLS=<n>.
//
// The Shell's own built-in commands (chd, chx, kill, x, ...) are done here:
// a Shell given only those would change nothing of ours, and would then
// wait for more input.  A command found on the path is loaded here, so the
// Shell finds it in memory, and unlinked when it has finished.  When tsh
// starts it runs /DD/SYS/tshrc; given a file, it runs that and exits; and
// with its input not a terminal, it runs what it reads.
//
// tsh reads keys through its own path to the terminal, with echo, the
// signal keys and the pause key turned off, so the Shell's settings are
// left alone.  On the TFR/911H's console, bit 7 of $FF42 gives the arrow
// keys as the tether sends them (128-131 Up Down Left Right, 132 and 133
// for Shift-Up and Shift-Down); any other terminal sends ANSI sequences.
// The screen is drawn with only Backspace, CR and printing characters, and
// a line longer than the screen scrolls sideways.
//
// Build for OS-9:  cmoc --os9 --function-stack=0 --add-os9-stack-space=3072 -o tsh tsh.c
// For testing on Unix:  cc -o tsh tsh.c   (commands go to /bin/sh;
//   TSH_EXECDIR, TSH_HISTORY and TSH_RC stand in for the OS-9 places)

#ifdef _CMOC_VERSION_
#include <cmoc.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <termios.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#endif

typedef unsigned char byte;
typedef unsigned short word;

#define BEL 7
#define BS 8
#define LF 10
#define CR 13
#define ESC 27

// Keys, as getkey() returns them.
#define K_UP 0x100
#define K_DOWN 0x101
#define K_LEFT 0x102
#define K_RIGHT 0x103
#define K_PGUP 0x104        // Shift-Up, or Page Up
#define K_PGDN 0x105
#define K_HOME 0x106
#define K_END 0x107
#define K_DEL 0x108
#define META 0x200          // Esc, then a character

#define LINEMAX 250         // longest command line
#define MAXCOLS 160
#define CMDMAX 400          // a line after the path is applied
#define HISTMAX 100         // lines of history kept
#define HISTSAVE 10         // lines kept back before adding them to the file
#define HISTBYTES 3000
#ifdef _CMOC_VERSION_
#define PATHBYTES 200
#define CWDMAX 128
#else
#define PATHBYTES 2000      // Unix pathlists are longer
#define CWDMAX 512
#endif
#define ALIASBYTES 600
#define CPOOL 2048          // completion candidates
#define NCAND 160
#define NJOBS 8             // commands running in the background
#define MODS 4              // modules loaded for one command line
#define DEPTH 4             // aliases and sourced files, inside each other

// OS-9 error codes, signals and access modes.
#define E_BMID 205
#define E_EOF 211
#define E_FNA 214
#define E_PNNF 216
#define E_NOCHLD 226
#define S_ABORT 2
#define S_INTRPT 3
#define S_HUP 4
#define READ_ 1
#define WRITE_ 2
#define EXEC_ 4
#define DIR_ 0x80

#ifdef _CMOC_VERSION_
#define HISTFILE "/DD/SYS/tsh_history"
#define RCFILE "/DD/SYS/tshrc"
#else
#define HISTFILE (getenv("TSH_HISTORY") ? getenv("TSH_HISTORY") : "tsh_history")
#define RCFILE (getenv("TSH_RC") ? getenv("TSH_RC") : "tshrc")
#endif
#define HOMEDIR "/DD"

void bye(byte code);
void ob_flush(void);

byte interactive;           // reading a terminal, with editing
char cwd[CWDMAX], prevcwd[CWDMAX];  // the data directory, now and before
byte outpath;               // where tsh's own output goes (set in main)
word oserr;                 // the last error from the OS
byte cols;

// ── Platform: NitrOS-9 ─────────────────────────────────────────────

#ifdef _CMOC_VERSION_

#define FF42 (*(byte *)0xFF42)

byte rawpath;               // our path to the terminal, for raw keys
byte shared;                // ... or the standard input, when that failed
byte is_tfr;                // the terminal is the TFR/911H's /Term
byte old_ff42;
byte sigs[2];               // the last signal, and whether one came
byte cooked[32], raw[32];   // the terminal's options, as found and for editing

asm __norts__ void intercept(void) {
    asm {
        stb     ,u
        lda     #1
        sta     1,u
        rti
    }
}

// OS-9 pathnames end at a CR.
void os9name(char *dst, const char *src) {
    byte i = 0;
    while (*src && i < CWDMAX - 2) dst[i++] = *src++;
    dst[i] = CR;
}

int f_open(const char *name, byte mode) {
    char n[CWDMAX];
    byte path = 0, err = 0;
    os9name(n, name);
    asm {
        lda     :mode
        leax    :n
        os9     $84             ; I$Open
        bcc     @ok
        stb     :err
        bra     @done
@ok     sta     :path
@done
    }
    oserr = err;
    if (err) return -1;         // (not "err ? -1 : path": CMOC makes that a byte)
    return path;
}

int f_create(const char *name) {
    char n[CWDMAX];
    byte path = 0, err = 0;
    os9name(n, name);
    asm {
        lda     #2              ; WRITE.
        ldb     #$0B            ; owner read+write, public read
        leax    :n
        os9     $83             ; I$Create
        bcc     @ok
        stb     :err
        bra     @done
@ok     sta     :path
@done
    }
    oserr = err;
    if (err) return -1;
    return path;
}

void f_close(byte path) {
    asm {
        lda     :path
        os9     $8F             ; I$Close
    }
}

// Returns the count read, or 0 at the end or on an error (in oserr).
word f_read(byte path, void *buf, word n, byte line) {
    word got = 0;
    byte err = 0;
    asm {
        lda     :path
        ldb     :line
        ldx     :buf
        pshs    y
        ldy     :n
        tstb
        bne     @ln
        os9     $89             ; I$Read
        bra     @chk
@ln     os9     $8B             ; I$ReadLn
@chk    bcc     @ok
        stb     :err
        ldy     #0
@ok     sty     :got
        puls    y
    }
    oserr = err;
    return got;
}

void f_write(byte path, const void *buf, word n) {
    byte err = 0;
    asm {
        lda     :path
        ldx     :buf
        pshs    y
        ldy     :n
        os9     $8A             ; I$Write
        puls    y
        bcc     @ok
        stb     :err
@ok
    }
    oserr = err;
}

// To the end of a file, to add to it.
void f_seekend(byte path) {
    asm {
        lda     :path
        pshs    u
        pshs    a
        ldb     #2              ; SS.Size: X:U
        os9     $8D             ; I$GetStt
        puls    a
        bcs     @err
        os9     $88             ; I$Seek
@err    puls    u
    }
}

// Cut a file to the given size.
void f_setsize(byte path, word size) {
    asm {
        lda     :path
        ldx     #0
        pshs    u
        ldu     :size
        ldb     #2              ; SS.Size
        os9     $8E             ; I$SetStt
        puls    u
    }
}

byte f_opts(byte path, byte *opts, byte set) {
    byte err = 0;
    asm {
        lda     :path
        ldx     :opts
        clrb                    ; SS.Opt
        tst     :set
        bne     @set
        os9     $8D             ; I$GetStt
        bra     @chk
@set    os9     $8E             ; I$SetStt
@chk    bcc     @ok
        stb     :err
@ok
    }
    return err;
}

// The name of the device a path is open on, without the slash.
void f_devname(byte path, char *name) {
    char buf[32];
    byte err = 0, i;
    asm {
        lda     :path
        ldb     #$0E            ; SS.DevNm
        leax    :buf
        os9     $8D             ; I$GetStt
        bcc     @ok
        stb     :err
@ok
    }
    name[0] = 0;
    if (err) return;
    for (i = 0; i < 31; i++) {
        name[i] = buf[i] & 0x7F;
        if (buf[i] & 0x80) break;
    }
    name[i + 1] = 0;
}

byte f_chdir(const char *name, byte exec) {
    char n[CWDMAX];
    byte err = 0, mode = exec ? EXEC_ | DIR_ : READ_ | DIR_;
    os9name(n, name);
    asm {
        lda     :mode
        leax    :n
        os9     $86             ; I$ChgDir
        bcc     @ok
        stb     :err
@ok
    }
    return err;
}

void perr(byte code) {
    ob_flush();
    asm {
        lda     #2
        ldb     :code
        os9     $0F             ; F$PErr
    }
}

// Start "Shell <line>".
byte p_fork(const char *line, word n, byte *pid) {
    static char param[CMDMAX + 2];
    char *pp = param;
    const char *shell = "Shell\r";
    byte err = 0, id = 0;
    memcpy(param, line, n);
    param[n++] = CR;
    asm {
        pshs    y,u
        ldd     :n
        ldx     :pp
        tfr     d,y             ; Y = the size of the parameters
        pshs    x
        ldx     :shell
        puls    u               ; U = the parameters
        lda     #$11            ; Prgrm+Objct
        clrb                    ; the data size of the module
        os9     $03             ; F$Fork
        puls    y,u
        bcc     @ok
        stb     :err
        bra     @done
@ok     sta     :id
@done
    }
    *pid = id;
    return err;
}

// Wait for a child to finish.  pid 0: a signal came first.
byte p_wait(byte *pid, byte *status) {
    byte err = 0, id = 0, st = 0;
    asm {
        os9     $04             ; F$Wait
        bcc     @ok
        stb     :err
        bra     @done
@ok     sta     :id
        stb     :st
@done
    }
    *pid = id;
    *status = st;
    return err;
}

byte p_send(byte pid, byte sig) {
    byte err = 0;
    asm {
        lda     :pid
        ldb     :sig
        os9     $08             ; F$Send
        bcc     @ok
        stb     :err
@ok
    }
    return err;
}

byte p_setpr(byte pid, byte prio) {
    byte err = 0;
    asm {
        lda     :pid
        ldb     :prio
        os9     $0D             ; F$SPrior
        bcc     @ok
        stb     :err
@ok
    }
    return err;
}

void p_exit(byte code) {
    asm {
        ldb     :code
        os9     $06             ; F$Exit
    }
}

// Is a module of this name in memory?
byte m_inmem(const char *name) {
    char n[CWDMAX];
    byte found = 0;
    os9name(n, name);
    asm {
        leax    :n
        pshs    y,u
        clra
        os9     $00             ; F$Link
        bcs     @no
        os9     $02             ; F$UnLink (U = the module)
        puls    y,u
        inc     :found
        bra     @done
@no     puls    y,u
@done
    }
    return found;
}

// Is there a file of this name in the execution directory?
byte m_inexec(const char *name) {
    int p = f_open(name, EXEC_);
    if (p < 0) return 0;
    f_close((byte)p);
    return 1;
}

// Load the module in a file, giving its address and copying its name.
word m_load(const char *path, char *modname) {
    char n[CWDMAX];
    word addr = 0;
    byte err = 0, i;
    const byte *mod, *nm;
    os9name(n, path);
    asm {
        leax    :n
        pshs    y,u
        clra
        os9     $01             ; F$Load: U = the module
        bcs     @err
        tfr     u,d
        puls    y,u
        std     :addr
        bra     @done
@err    puls    y,u
        stb     :err
@done
    }
    oserr = err;
    if (err) return 0;
    mod = (const byte *)addr;
    nm = mod + (((word)mod[4] << 8) | mod[5]);
    for (i = 0; i < 29; i++) {
        modname[i] = nm[i] & 0x7F;
        if (nm[i] & 0x80) break;
    }
    modname[i + 1] = 0;
    return addr;
}

void m_unlink(word addr) {
    asm {
        pshs    u
        ldu     :addr
        os9     $02             ; F$UnLink
        puls    u
    }
}

// The time: year (since 1900), month, day, hour, minute, second.
void t_now(byte *t) {
    asm {
        ldx     :t
        os9     $15             ; F$Time
    }
}

// Directories: a path open on one, and its next name.
int d_open(const char *dir, byte exec) {
    return f_open(dir, exec ? DIR_ | READ_ | EXEC_ : DIR_ | READ_);
}

byte d_next(int d, char *name) {
    byte ent[32], i;
    while (f_read((byte)d, ent, 32, 0) == 32) {
        if (!ent[0]) continue;              // deleted
        for (i = 0; i < 29; i++) {
            name[i] = ent[i] & 0x7F;
            if (ent[i] & 0x80) break;
        }
        name[i + 1] = 0;
        if (name[0] == '.' && (!name[1] || (name[1] == '.' && !name[2]))) continue;
        return 1;
    }
    return 0;
}

void d_close(int d) { f_close((byte)d); }

byte is_dir(const char *path) {
    int p = f_open(path, DIR_ | READ_);
    if (p < 0) return 0;
    f_close((byte)p);
    return 1;
}

// The data directory's pathlist, found as pwd finds it: each directory's
// name is in its parent, under the number of its first sector.
void cwd_find(char *dst) {
    char rel[48], name[32], out[CWDMAX];
    byte ent[32], self[3], parent[3], grand[3], depth, found, n, on;
    int p;
    on = CWDMAX - 1;
    out[on] = 0;
    strcpy(rel, ".");
    p = f_open(rel, DIR_ | READ_);
    if (p < 0) { strcpy(dst, "?"); return; }
    f_read((byte)p, ent, 32, 0);            // ".."
    memcpy(parent, ent + 29, 3);
    f_read((byte)p, ent, 32, 0);            // "."
    memcpy(self, ent + 29, 3);
    for (depth = 0; depth < 12 && memcmp(self, parent, 3); depth++) {
        f_close((byte)p);
        if (depth) strcat(rel, "/..");
        else strcpy(rel, "..");
        p = f_open(rel, DIR_ | READ_);
        if (p < 0) { strcpy(dst, "?"); return; }
        found = 0;
        memset(grand, 0, 3);
        while (!found && f_read((byte)p, ent, 32, 0) == 32) {
            if (!grand[0] && !grand[1] && !grand[2]) memcpy(grand, ent + 29, 3);
            if (ent[0] && !memcmp(ent + 29, self, 3) && ent[0] != ('.' | 0x80)
                && !(ent[0] == '.' && ent[1] == ('.' | 0x80))) {
                for (n = 0; n < 29; n++) {
                    name[n] = ent[n] & 0x7F;
                    if (ent[n] & 0x80) break;
                }
                name[++n] = 0;
                found = 1;
            }
        }
        if (!found || n + 1 >= on) { f_close((byte)p); strcpy(dst, "?"); return; }
        on -= n;
        memcpy(out + on, name, n);
        out[--on] = '/';
        memcpy(self, parent, 3);
        memcpy(parent, grand, 3);
    }
    f_devname((byte)p, name);
    f_close((byte)p);
    dst[0] = '/';
    strcpy(dst + 1, name);
    strcat(dst, out + on);
}

// Returns 1 if the standard input is a terminal, to edit lines on.
byte tty_init(void) {
    char dev[34];
    byte *flag = sigs;
    void (*handler)(void) = intercept;
    int p;
    asm {
        ldx     :handler
        pshs    u
        ldu     :flag
        os9     $09             ; F$Icpt
        puls    u
    }
    if (f_opts(0, cooked, 0) || cooked[0] != 0) return 0;   // not SCF
    dev[0] = '/';
    f_devname(0, dev + 1);
    is_tfr = !stricmp(dev + 1, "term");
    p = dev[1] ? f_open(dev, READ_ | WRITE_) : -1;
    if (p < 0) { p = 0; shared = 1; }       // edit on the standard input
    rawpath = (byte)p;
    f_opts(rawpath, raw, 0);
    memcpy(cooked, raw, 32);
    raw[1] = 0;                 // PD.UPC: both cases
    raw[4] = 0;                 // PD.EKO: no echo
    raw[5] = 0;                 // PD.ALF: no line feed after CR
    raw[7] = 0;                 // PD.PAU: no page pause
    raw[12] = 0;                // PD.EOF
    raw[15] = 0;                // PD.PSC: ^W is ours
    raw[16] = 0;                // PD.INT: ^C is ours
    raw[17] = 0;                // PD.QUT: ^E is ours
    if (!shared) f_opts(rawpath, raw, 1);
    outpath = rawpath;
    old_ff42 = is_tfr ? FF42 : 0;
    return 1;
}

// Keys for editing, or the terminal as the commands want it.
void tty_edit(byte on) {
    if (is_tfr) FF42 = on ? (FF42 | 0x80) : (old_ff42 & 0x7F);
    if (shared) f_opts(0, on ? raw : cooked, 1);
}

void tty_done(void) {
    if (!interactive) return;
    tty_edit(0);
    if (!shared) f_close(rawpath);
}

int key_read(void) {
    byte c;
    for (;;) {
        if (f_read(rawpath, &c, 1, 0)) return c;
        if (oserr != S_ABORT && oserr != S_INTRPT) return -1;
        if (sigs[1] && sigs[0] == S_HUP) return -1;
    }
}

// The newline after a command line.  A write on the standard output, even
// of nothing, gives the terminal back its signal keys (SCF copies them
// from the path that uses the device).
void tty_newline(void) {
    f_write(rawpath, "\r\n", 2);
    f_write(1, "", 0);
}

void out_raw(const char *p, word n) { f_write(outpath, p, n); }

void plat_init(void) {}

// ── Platform: Unix (for testing) ───────────────────────────────────

#else

struct termios saved_tio;
byte is_tfr = 1;            // take bytes 128-133 as the tether's keys
char execdir[CWDMAX] = ".";
#define stricmp strcasecmp
#define memicmp mem_icmp

int mem_icmp(const void *a, const void *b, size_t n) {
    const byte *p = a, *q = b;
    for (; n; n--, p++, q++)
        if (tolower(*p) != tolower(*q)) return tolower(*p) - tolower(*q);
    return 0;
}

void plat_init(void) {
    char *e = getenv("TSH_EXECDIR");
    if (e) strncpy(execdir, e, sizeof execdir - 1);
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
}

int f_open(const char *name, byte mode) {
    int fd = open(name, (mode & 3) == 3 ? O_RDWR : (mode & WRITE_) ? O_WRONLY : O_RDONLY);
    oserr = fd < 0 ? (errno == ENOENT ? E_PNNF : E_FNA) : 0;
    return fd;
}

int f_create(const char *name) {
    int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    oserr = fd < 0 ? E_FNA : 0;
    return fd;
}

void f_close(byte path) { close(path); }

word f_read(byte path, void *buf, word n, byte line) {
    word got = 0;
    char *b = buf;
    ssize_t r;
    oserr = 0;
    if (!line) {
        r = read(path, buf, n);
        if (r <= 0) oserr = E_EOF;
        return r > 0 ? (word)r : 0;
    }
    while (got < n && read(path, b + got, 1) == 1) {
        if (b[got] == '\n') b[got] = CR;
        if (b[got++] == CR) return got;
    }
    if (!got) oserr = E_EOF;
    return got;
}

void f_write(byte path, const void *buf, word n) {
    oserr = write(path, buf, n) < 0 ? E_FNA : 0;
}

void f_seekend(byte path) { lseek(path, 0, SEEK_END); }
void f_setsize(byte path, word size) { if (ftruncate(path, size)) {} }

byte f_chdir(const char *name, byte exec) {
    struct stat st;
    if (exec) {
        if (stat(name, &st) || !S_ISDIR(st.st_mode)) return E_PNNF;
        strncpy(execdir, name, sizeof execdir - 1);
        return 0;
    }
    return chdir(name) ? E_PNNF : 0;
}

void perr(byte code) {
    char b[24];
    ob_flush();
    snprintf(b, sizeof b, "ERROR #%03d\r\n", code);
    if (write(1, b, strlen(b)) < 0) {}
}

int pids[32];

byte p_fork(const char *line, word n, byte *pid) {
    char cmd[CMDMAX + 1];
    int i, k;
    memcpy(cmd, line, n);
    cmd[n] = 0;
    for (i = 0; i < 32 && pids[i]; i++) {}
    if (i == 32) return 200;
    k = fork();
    if (k < 0) return 207;
    if (!k) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        execl("/bin/sh", "sh", "-c", cmd, (char *)0);
        _exit(127);
    }
    pids[i] = k;
    *pid = (byte)(i + 1);
    return 0;
}

byte p_wait(byte *pid, byte *status) {
    int st, k, i;
    k = waitpid(-1, &st, 0);
    if (k < 0) {
        *pid = 0;
        return errno == ECHILD ? E_NOCHLD : 0;
    }
    for (i = 0; i < 32 && pids[i] != k; i++) {}
    if (i < 32) pids[i] = 0;
    *pid = i < 32 ? (byte)(i + 1) : 99;
    *status = WIFEXITED(st) ? (byte)WEXITSTATUS(st) : WTERMSIG(st) == SIGINT ? S_INTRPT : S_ABORT;
    return 0;
}

byte p_send(byte pid, byte sig) {
    if (pid < 1 || pid > 32 || !pids[pid - 1]) return 238;
    kill(pids[pid - 1], sig == S_INTRPT ? SIGINT : SIGKILL);
    return 0;
}

byte p_setpr(byte pid, byte prio) { (void)pid; (void)prio; return 0; }
void p_exit(byte code) { exit(code); }

byte m_inmem(const char *name) { (void)name; return 0; }

byte m_inexec(const char *name) {
    char p[2 * CWDMAX];
    snprintf(p, sizeof p, "%s/%s", execdir, name);
    return !access(p, X_OK);
}

// A program on the path: there are no modules to load here, so the
// pathlist is given to /bin/sh (as for a procedure file on OS-9).
word m_load(const char *path, char *modname) {
    (void)modname;
    oserr = access(path, F_OK) ? E_PNNF : E_BMID;
    return 0;
}

void m_unlink(word addr) { (void)addr; }

void t_now(byte *t) {
    time_t now = time(0);
    struct tm *tm = localtime(&now);
    t[0] = tm->tm_year; t[1] = tm->tm_mon + 1; t[2] = tm->tm_mday;
    t[3] = tm->tm_hour; t[4] = tm->tm_min; t[5] = tm->tm_sec;
}

DIR *dirs[4];

int d_open(const char *dir, byte exec) {
    int i;
    for (i = 0; i < 4 && dirs[i]; i++) {}
    if (i == 4 || !(dirs[i] = opendir(exec && !strcmp(dir, ".") ? execdir : dir))) return -1;
    return i;
}

byte d_next(int d, char *name) {
    struct dirent *e;
    while ((e = readdir(dirs[d]))) {
        if (e->d_name[0] == '.') continue;
        strncpy(name, e->d_name, 29);
        name[29] = 0;
        return 1;
    }
    return 0;
}

void d_close(int d) { closedir(dirs[d]); dirs[d] = 0; }

byte is_dir(const char *path) {
    struct stat st;
    return !stat(path, &st) && S_ISDIR(st.st_mode);
}

void cwd_find(char *cwd) { if (!getcwd(cwd, CWDMAX)) strcpy(cwd, "?"); }

byte tty_init(void) {
    if (!isatty(0)) return 0;
    tcgetattr(0, &saved_tio);
    return 1;
}

void tty_edit(byte on) {
    struct termios t = saved_tio;
    if (on) {
        cfmakeraw(&t);
        tcsetattr(0, TCSANOW, &t);
    } else {
        tcsetattr(0, TCSANOW, &saved_tio);
    }
}

void tty_done(void) { if (interactive) tty_edit(0); }

int key_read(void) {
    byte c;
    return read(0, &c, 1) == 1 ? c : -1;
}

void tty_newline(void) { if (write(1, "\r\n", 2) < 0) {} }

void out_raw(const char *p, word n) { if (write(outpath, p, n) < 0) {} }

#endif

// ── Output ─────────────────────────────────────────────────────────

char obuf[255];
byte olen;

void ob_flush(void) {
    if (olen) out_raw(obuf, olen);
    olen = 0;
}

void put(char c) {
    if (olen == sizeof obuf) ob_flush();
    obuf[olen++] = c;
}

// A string, where \n is a newline.
void out(const char *s) {
    for (; *s; s++) {
        if (*s != '\n') put(*s);
        else if (interactive) { put(CR); put(LF); }
        else put(CR);
    }
}

void outn(const char *s, byte n) {
    while (n--) put(*s++);
}

void outnum(word v, byte width) {
    char b[6];
    byte n = 0;
    do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (width-- > n) put('0');
    while (n) put(b[--n]);
}

void beep(void) { put(BEL); ob_flush(); }

// ── Small helpers ──────────────────────────────────────────────────

byte lower(byte c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

// Does s begin with the n characters of p, in either case?
byte starts(const char *s, const char *p, byte n) {
    for (; n; n--, s++, p++)
        if (lower(*s) != lower(*p)) return 0;
    return 1;
}

// Is p (of n characters) in s, in either case?
byte contains(const char *s, const char *p, byte n) {
    for (; *s; s++)
        if (starts(s, p, n) && strlen(s) >= n) return 1;
    return !n;
}

char *skipsp(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

// Split s into words, in place: spaces separate, double quotes group.
byte split(char *s, char **argv, byte max) {
    byte n = 0;
    char *d;
    for (;;) {
        s = skipsp(s);
        if (!*s || n == max) return n;
        argv[n++] = d = s;
        while (*s && *s != ' ' && *s != '\t') {
            if (*s == '"') {
                for (s++; *s && *s != '"'; ) *d++ = *s++;
                if (*s) s++;
            } else *d++ = *s++;
        }
        if (*s) s++;
        *d = 0;
    }
}

// The text of s, without the double quotes around it.
char *unquote(char *s) {
    word n;
    s = skipsp(s);
    n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = 0;
    if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
        s[n - 1] = 0;
        s++;
    }
    return s;
}

word number(const char *s) {
    word v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (word)(*s++ - '0');
    return v;
}

// ── Settings: the path, aliases, the prompt ────────────────────────

char pathv[PATHBYTES];      // directories, each ended by a NUL
word pathlen;
byte npath;

char aliasv[ALIASBYTES];    // name NUL text NUL, for each alias
word aliaslen;
int abusy[DEPTH];           // the aliases being expanded, not to be again

char promptfmt[80];
byte status;                // the last command's exit status
byte opt_x, opt_t, opt_nop; // Shell options: stop on error, echo lines, no prompt

void path_clear(void) { pathlen = 0; npath = 0; }

const char *path_next(const char *p) { return p + strlen(p) + 1; }

void path_del(const char *dir) {
    char *p = pathv;
    byte i;
    word n;
    for (i = 0; i < npath; i++, p += n) {
        n = strlen(p) + 1;
        if (!stricmp(p, dir)) {
            memmove(p, p + n, pathlen - (word)(p - pathv) - n);
            pathlen -= n;
            npath--;
            return;
        }
    }
}

// A directory at the end of the path.  A relative one is taken from the
// data directory now, since F$Load would take it from the execution one.
void path_add(const char *dir, word n) {
    char d[CWDMAX];
    word k = 0;
    if (!n) return;
    if (*dir != '/') {
        if (!cwd[0]) cwd_find(cwd);
        k = strlen(cwd);
        if (!strcmp(cwd, "?")) k = 0;
    }
    if (n + k + 1 >= CWDMAX - 32) {
        out("tsh: too long for the path\n");
        return;
    }
    memcpy(d, cwd, k);
    if (k) d[k++] = '/';
    memcpy(d + k, dir, n);
    n += k;
    d[n] = 0;
    while (n > 1 && d[n - 1] == '/') d[--n] = 0;
    path_del(d);
    if (pathlen + n + 1 > PATHBYTES) {
        out("tsh: the path is full\n");
        return;
    }
    memcpy(pathv + pathlen, d, n + 1);
    pathlen += n + 1;
    npath++;
}

// "PATH=/DD/CMDS:/H0/CMDS" or with spaces.
void path_set(const char *s) {
    const char *e;
    path_clear();
    for (;;) {
        while (*s == ' ' || *s == ':') s++;
        if (!*s) return;
        for (e = s; *e && *e != ' ' && *e != ':'; e++) {}
        path_add(s, (word)(e - s));
        s = e;
    }
}

void show_path(char sep) {
    const char *p = pathv;
    byte i;
    if (!npath && sep == ' ') out("(none: memory, then the execution directory)");
    for (i = 0; i < npath; i++, p = path_next(p)) {
        if (i) put(sep);
        out(p);
    }
    out("\n");
}

int alias_find(const char *name, byte n) {
    const char *p = aliasv;
    int i = 0;
    while (p < aliasv + aliaslen) {
        if (strlen(p) == n && starts(p, name, n)) return i;
        p = path_next(path_next(p));
        i++;
    }
    return -1;
}

char *alias_at(int i) {
    char *p = aliasv;
    while (i--) p = (char *)path_next(path_next(p));
    return p;
}

void alias_del(const char *name) {
    int i = alias_find(name, (byte)strlen(name));
    char *p, *e;
    if (i < 0) return;
    p = alias_at(i);
    e = (char *)path_next(path_next(p));
    memmove(p, e, aliaslen - (word)(e - aliasv));
    aliaslen -= (word)(e - p);
}

void alias_set(const char *name, const char *text) {
    word n = strlen(name) + 1, t = strlen(text) + 1;
    alias_del(name);
    if (aliaslen + n + t > ALIASBYTES) {
        out("tsh: no room for more aliases\n");
        return;
    }
    memcpy(aliasv + aliaslen, name, n);
    memcpy(aliasv + aliaslen + n, text, t);
    aliaslen += n + t;
}

void alias_show(const char *p) {
    out("alias ");
    out(p);
    out(" ");
    out(path_next(p));
    out("\n");
}

// ── History ────────────────────────────────────────────────────────

char hpool[HISTBYTES];      // the lines, oldest first, each ended by a NUL
word hstart[HISTMAX];
word hused;
byte hcount;
word hbase;                 // the number of the oldest line kept
byte hunsaved;              // the newest lines, not yet in the file

const char *hist_get(byte i) { return hpool + hstart[i]; }

void hist_drop(void) {
    word n = strlen(hpool) + 1;
    byte i;
    memmove(hpool, hpool + n, hused - n);
    hused -= n;
    hcount--;
    for (i = 0; i < hcount; i++) hstart[i] = hstart[i + 1] - n;
    hbase++;
}

// Returns 1 if the line was added (not a repeat of the last).
byte hist_add(const char *s) {
    word n = strlen(s) + 1;
    if (n == 1 || (hcount && !strcmp(s, hist_get(hcount - 1)))) return 0;
    while (hcount && (hcount == HISTMAX || hused + n > HISTBYTES)) hist_drop();
    hstart[hcount++] = hused;
    memcpy(hpool + hused, s, n);
    hused += n;
    return 1;
}

int hist_open(void) {
    int p = f_open(HISTFILE, WRITE_);
    if (p < 0) p = f_create(HISTFILE);
    return p;
}

// Write the history file again, with just the lines kept.
void hist_rewrite(void) {
    int p = hist_open();
    word size = 0;
    byte i;
    const char *s;
    if (p < 0) return;
    for (i = 0; i < hcount; i++) {
        s = hist_get(i);
        f_write((byte)p, s, strlen(s));
        f_write((byte)p, "\r", 1);
        size += strlen(s) + 1;
    }
    f_setsize((byte)p, size);
    f_close((byte)p);
}

// Add the newest lines to the file.  Not after every line: each time is
// a few sectors written, and while the board's terminal is busy that is
// what most often loses a byte on the way to the tether.  tsh keeps
// running when the tether quits, so only a reset loses any.
void hist_flush(void) {
    int p;
    byte i;
    const char *s;
    if (!hunsaved) return;
    p = hist_open();
    if (p < 0) return;
    f_seekend((byte)p);
    for (i = hcount - hunsaved; i < hcount; i++) {
        s = hist_get(i);
        f_write((byte)p, s, strlen(s));
        f_write((byte)p, "\r", 1);
    }
    f_close((byte)p);
    hunsaved = 0;
}

void hist_load(void) {
    char b[LINEMAX + 2];
    word n, lines = 0;
    int p = f_open(HISTFILE, READ_);
    if (p < 0) return;
    while ((n = f_read((byte)p, b, LINEMAX + 1, 1)) > 0) {
        while (n && (b[n - 1] == CR || b[n - 1] == LF)) n--;
        b[n] = 0;
        hist_add(b);
        lines++;
    }
    f_close((byte)p);
    if (lines > hcount + 20) hist_rewrite();     // let it grow a little
}

// ── The line editor ────────────────────────────────────────────────

char line[LINEMAX + 1];
byte len, cur;              // the line's length, and the cursor in it
byte hoff;                  // the first character shown
char saved[LINEMAX + 1];    // the new line, while looking at the history
char killbuf[LINEMAX + 1];
char prompt[80];
byte plen;
char shadow[MAXCOLS];       // what the row on the screen shows
byte shlen, scol;           // its length, and the cursor's column
int pending;                // a key to take again, or -1

int getkey(void) {
    int c, f;
    byte n, m, second;
    if (pending >= 0) {
        c = pending;
        pending = -1;
        return c;
    }
    c = key_read();
    if (c >= 128 && c <= 133) return is_tfr ? K_UP + (c - 128) : 0;
    if (c != ESC) return c;
    c = key_read();
    if (c != '[' && c != 'O') return c < 0 ? c : META | c;
    // An ANSI sequence: a number or two, then a letter or ~.
    n = m = second = 0;
    while ((f = key_read()) >= '0' && f <= '?') {
        if (f == ';') second = 1;           // then the modifier keys: 2 is Shift
        else if (f >= '0' && f <= '9' && !second && n < 20) n = (byte)(n * 10 + (f - '0'));
        else if (f >= '0' && f <= '9' && m < 20) m = (byte)(m * 10 + (f - '0'));
    }
    switch (f) {
    case 'A': return m == 2 ? K_PGUP : K_UP;
    case 'B': return m == 2 ? K_PGDN : K_DOWN;
    case 'C': return K_RIGHT;
    case 'D': return K_LEFT;
    case 'H': return K_HOME;
    case 'F': return K_END;
    case '~':
        switch (n) {
        case 1: case 7: return K_HOME;
        case 4: case 8: return K_END;
        case 3: return K_DEL;
        case 5: return K_PGUP;
        case 6: return K_PGDN;
        }
    }
    return f < 0 ? f : 0;
}

// Move the screen's cursor to column c of the row.
void moveto(byte c) {
    if (c < scol) {
        if ((word)(scol - c) > (word)c + 1) {
            put(CR);
            scol = 0;
        } else {
            for (; scol > c; scol--) put(BS);
        }
    }
    for (; scol < c; scol++) put(scol < shlen ? shadow[scol] : ' ');
}

// The columns for text when the line is shown from character h: all
// after the prompt but the last, less one for a < (more on the left) and
// one for a > (more on the right).
byte avail;

byte textw(byte h) {
    byte w = avail - (h ? 1 : 0);
    return len - h > w ? w - 1 : w;
}

// Bring the row up to date: what changed, then the cursor.
void refresh(void) {
    char want[MAXCOLS];
    byte n, i, w;
    avail = cols - 1 - plen;
    if (len < avail) hoff = 0;
    else if (cur < hoff || cur - hoff >= textw(hoff)) {
        hoff = cur > avail / 2 ? cur - avail / 2 : 0;   // half a screen at a time
        while (cur - hoff >= textw(hoff)) hoff++;
    }
    w = textw(hoff);
    memcpy(want, prompt, plen);
    n = plen;
    if (hoff) want[n++] = '<';
    for (i = hoff; i < len && i - hoff < w; i++) want[n++] = line[i];
    if (len - hoff > w) want[n++] = '>';
    for (i = 0; i < n && i < shlen && want[i] == shadow[i]; i++) {}
    if (i < n || i < shlen) {
        moveto(i);
        for (; i < n; i++) put(shadow[i] = want[i]);
        scol = n;
        for (; i < shlen; i++, scol++) put(' ');
        shlen = n;
    }
    moveto((byte)(plen + (hoff ? 1 : 0) + cur - hoff));
    ob_flush();
}

// Leave the row: the cursor after the line, then a new line.
void newrow(void) {
    moveto(shlen);
    put(CR);
    put(LF);
    shlen = scol = 0;
}

void set_line(const char *s) {
    len = (byte)strlen(s);
    if (len > LINEMAX) len = LINEMAX;
    memcpy(line, s, len);
    line[len] = 0;
    cur = len;
}

void ins_text(const char *s, byte n) {
    if (n > LINEMAX - len) n = LINEMAX - len;
    memmove(line + cur + n, line + cur, len - cur);
    memcpy(line + cur, s, n);
    len += n;
    cur += n;
    line[len] = 0;
}

void del_text(byte at, byte n) {
    memmove(line + at, line + at + n, len - at - n);
    len -= n;
    line[len] = 0;
    if (cur >= at + n) cur -= n;
    else if (cur > at) cur = at;
}

void kill_text(byte at, byte n) {
    if (!n) return;
    memcpy(killbuf, line + at, n);
    killbuf[n] = 0;
    del_text(at, n);
}

byte is_word(byte c) { return isalnum(c) || c == '_'; }

byte word_left(byte i) {
    while (i && !is_word(line[i - 1])) i--;
    while (i && is_word(line[i - 1])) i--;
    return i;
}

byte word_right(byte i) {
    while (i < len && !is_word(line[i])) i++;
    while (i < len && is_word(line[i])) i++;
    return i;
}

void use_prompt(const char *s) {
    plen = (byte)strlen(s);
    if (plen > cols - 20) plen = cols - 20;
    memcpy(prompt, s, plen);
}

// Expand the prompt's %d (directory), %w (its last name), %h (history
// number), %e (exit status), %t (time) and %%.
void make_prompt(void) {
    char p[CWDMAX + 80], *s;
    const char *f;
    byte n = 0, t[6];
    word v;
    for (f = promptfmt; *f && n < sizeof p - CWDMAX; f++) {
        if (*f != '%' || !f[1]) {
            p[n++] = *f;
            continue;
        }
        v = 0xFFFF;
        switch (*++f) {
        case 'd': strcpy(p + n, cwd); n += strlen(cwd); break;
        case 'w':
            s = strrchr(cwd, '/');
            strcpy(p + n, s && s[1] ? s + 1 : cwd);
            n += strlen(p + n);
            break;
        case 'h': v = hbase + hcount; break;
        case 'e': v = status; break;
        case 't':
            t_now(t);
            p[n++] = (char)('0' + t[3] / 10); p[n++] = (char)('0' + t[3] % 10);
            p[n++] = ':';
            p[n++] = (char)('0' + t[4] / 10); p[n++] = (char)('0' + t[4] % 10);
            break;
        default: p[n++] = *f;
        }
        if (v != 0xFFFF) {
            char b[6];
            byte k = 0;
            do { b[k++] = (char)('0' + v % 10); v /= 10; } while (v);
            while (k) p[n++] = b[--k];
        }
    }
    p[n] = 0;
    // A long prompt keeps its end.
    if (n > cols - 20) {
        s = p + n - (cols - 21);
        *--s = '<';
    } else s = p;
    use_prompt(opt_nop ? "" : s);
}

// ── Completion ─────────────────────────────────────────────────────

char cpool[CPOOL];
char *cand[NCAND];
word cused;
byte ncand, cfull;

const char builtins[] =
    "cd chd chx path history alias unalias source . which set "
    "w wait kill setpr x -x p -p t -t ex exit ";

// The n-th word of a list like builtins, into w.
byte list_word(const char *list, byte i, char *w) {
    byte n;
    while (i--) {
        while (*list && *list != ' ') list++;
        if (*list) list++;
    }
    for (n = 0; list[n] && list[n] != ' '; n++) w[n] = list[n];
    w[n] = 0;
    return n;
}

// Which built-in command is s (of n characters)?  -1: none.
int builtin_find(const char *s, word n) {
    char b[12];
    byte i, k;
    for (i = 0; (k = list_word(builtins, i, b)) != 0; i++)
        if (k == n && starts(s, b, k)) return i;
    return -1;
}

void cand_add(const char *name, const char *pre, byte pn) {
    word n = strlen(name) + 1;
    byte i;
    if (!starts(name, pre, pn) || strlen(name) < pn) return;
    for (i = 0; i < ncand; i++)
        if (lower(*cand[i]) == lower(*name) && !stricmp(cand[i], name)) return;
    if (ncand == NCAND || cused + n > CPOOL) {
        cfull = 1;
        return;
    }
    cand[ncand++] = cpool + cused;
    memcpy(cpool + cused, name, n);
    cused += n;
}

void cand_dir(const char *dir, byte exec, const char *pre, byte pn) {
    char name[32];
    int d = d_open(dir, exec);
    if (d < 0) return;
    while (d_next(d, name)) cand_add(name, pre, pn);
    d_close(d);
}

void list_cands(void) {
    byte i, k, w = 0, per, r, c, rows;
    char *t;
    word at;
    for (i = 1; i < ncand; i++) {           // sort them
        t = cand[i];
        for (k = i; k && stricmp(cand[k - 1], t) > 0; k--) cand[k] = cand[k - 1];
        cand[k] = t;
    }
    for (i = 0; i < ncand; i++)
        if (strlen(cand[i]) > w) w = (byte)strlen(cand[i]);
    w += 2;
    per = (byte)((cols - 1) / w);
    if (!per) per = 1;
    rows = (byte)(((word)ncand + per - 1) / per);
    newrow();
    for (r = 0; r < rows; r++) {
        for (c = 0; c < per; c++) {
            at = (word)c * rows + r;
            if (at >= ncand) break;
            out(cand[at]);
            if (c + 1 < per && at + rows < ncand)
                for (k = (byte)strlen(cand[at]); k < w; k++) put(' ');
        }
        out("\n");
    }
    if (cfull) out("(and more)\n");
    refresh();
}

#define BREAKS " \t;&!()<>,"

// Complete the word before the cursor: a command name at the start of a
// command, else a file name.  The second Tab in a row lists the choices.
void complete(byte again) {
    char dir[CWDMAX + 32], w[32];
    const char *pre, *p;
    byte ws, k, sl, pn, n, i, cmdpos;
    for (ws = cur; ws && !strchr(BREAKS, line[ws - 1]); ws--) {}
    for (k = ws; k && line[k - 1] == ' '; k--) {}
    cmdpos = !k || strchr(";&!(", line[k - 1]) != 0;
    for (sl = cur; sl > ws && line[sl - 1] != '/'; sl--) {}
    pre = line + sl;
    pn = cur - sl;
    ncand = 0;
    cused = 0;
    cfull = 0;
    if (sl > ws) {                          // in a directory given
        n = sl - ws - 1;
        if (!n || (word)n >= CWDMAX) { beep(); return; }
        memcpy(dir, line + ws, n);
        dir[n] = 0;
        cand_dir(dir, 0, pre, pn);
    } else if (cmdpos) {
        for (i = 0; (k = list_word(builtins, i, w)) != 0; i++)
            if (k > 1 && w[0] != '-') cand_add(w, pre, pn);
        for (p = aliasv; p < aliasv + aliaslen; p = path_next(path_next(p))) cand_add(p, pre, pn);
        cand_dir(".", 1, pre, pn);
        for (i = 0, p = pathv; i < npath; i++, p = path_next(p)) cand_dir(p, 0, pre, pn);
    } else {
        cand_dir(".", 0, pre, pn);
    }
    if (!ncand) { beep(); return; }
    n = (byte)strlen(cand[0]);              // what they all start with
    for (i = 1; i < ncand; i++) {
        for (k = 0; k < n && lower(cand[i][k]) == lower(cand[0][k]); k++) {}
        n = k;
    }
    if (ncand == 1 || n > pn) {
        cur = sl + pn;
        del_text(sl, pn);                   // the name's own spelling
        ins_text(cand[0], n);
        if (ncand == 1) {
            if (!cmdpos || sl > ws) {
                k = sl - ws;
                memcpy(dir, line + ws, k + n);
                dir[k + n] = 0;
                ins_text(is_dir(dir) ? "/" : " ", 1);
            } else ins_text(" ", 1);
        }
        refresh();
        return;
    }
    if (again) list_cands();
    else beep();
}

// ── History keys ───────────────────────────────────────────────────

byte hpos;                  // the history line shown; hcount for the new one

void hist_move(int dir) {
    if (dir < 0 ? !hpos : hpos == hcount) { beep(); return; }
    if (hpos == hcount) strcpy(saved, line);
    hpos += dir;
    set_line(hpos == hcount ? saved : hist_get(hpos));
}

// The next older or newer line that starts with what is before the cursor.
void hist_prefix(int dir) {
    int i = hpos;
    byte n = cur;
    for (;;) {
        i += dir;
        if (i < 0 || i > hcount) { beep(); return; }
        if (i == hcount) break;
        if (starts(hist_get((byte)i), line, n) && strcmp(hist_get((byte)i), line)) break;
    }
    if (hpos == hcount) strcpy(saved, line);
    hpos = (byte)i;
    set_line(hpos == hcount ? saved : hist_get(hpos));
    cur = n;
}

// ^R: search back through the history as the text is typed.
void isearch(void) {
    char pat[40], orig[LINEMAX + 1], p[80];
    byte pn = 0, at = hpos, ok = 1, from, k;
    int c, i;
    strcpy(orig, line);
    for (;;) {
        strcpy(p, ok ? "(search)'" : "(failed search)'");
        k = (byte)strlen(p);
        memcpy(p + k, pat, pn);
        strcpy(p + k + pn, "': ");
        use_prompt(p);
        refresh();
        c = getkey();
        from = at;
        if (c == 18) {                      // ^R: an older one
            if (!from) { beep(); continue; }
            from--;
        } else if (c == BS || c == 127) {
            if (pn) pn--;
            continue;
        } else if (c == 3 || c == 7) {      // ^C, ^G: as it was
            set_line(orig);
            break;
        } else if (c >= ' ' && c < 127) {
            if (pn < sizeof pat) pat[pn++] = (char)c;
        } else {
            pending = c;                    // any other key: done, and use it
            break;
        }
        for (i = from; i >= 0; i--) {
            if (i < hcount && contains(hist_get((byte)i), pat, pn)) break;
        }
        ok = i >= 0;
        if (ok) {
            at = (byte)i;
            if (at < hcount) {
                if (hpos == hcount) strcpy(saved, orig);
                hpos = at;
                set_line(hist_get(at));
            }
        }
    }
    make_prompt();
    refresh();
}

// ── Reading a line ─────────────────────────────────────────────────

// Returns 0 at the end of the input.
byte edit_line(void) {
    int c, last = 0;
    byte i;
    len = cur = hoff = 0;
    line[0] = 0;
    hpos = hcount;
    shlen = scol = 0;
    refresh();
    for (;; last = c) {
        c = getkey();
        if (c < 0) return 0;
        switch (c) {
        case CR: case LF:
            cur = len;
            refresh();
            tty_newline();
            shlen = scol = 0;
            return 1;
        case 3:                             // ^C
            cur = len;
            refresh();
            out("^C");
            ob_flush();
            tty_newline();
            len = 0;
            line[0] = 0;
            return 1;
        case 1: case K_HOME: cur = 0; break;
        case 5: case K_END: cur = len; break;
        case 2: case K_LEFT: if (cur) cur--; break;
        case 6: case K_RIGHT: if (cur < len) cur++; break;
        case META | 'b': case META | 'B': cur = word_left(cur); break;
        case META | 'f': case META | 'F': cur = word_right(cur); break;
        case BS: case 127: if (cur) del_text(cur - 1, 1); break;
        case 4: case K_DEL: if (cur < len) del_text(cur, 1); break;
        case 23:                            // ^W: back to a space
            for (i = cur; i && line[i - 1] == ' '; i--) {}
            for (; i && line[i - 1] != ' '; i--) {}
            kill_text(i, cur - i);
            break;
        case META | BS: case META | 127:
            i = word_left(cur);
            kill_text(i, cur - i);
            break;
        case META | 'd': case META | 'D':
            kill_text(cur, word_right(cur) - cur);
            break;
        case 11: kill_text(cur, len - cur); break;     // ^K
        case 21: kill_text(0, cur); break;             // ^U
        case 24: cur = len; kill_text(0, len); break;  // ^X
        case 25: ins_text(killbuf, (byte)strlen(killbuf)); break;   // ^Y
        case 20:                            // ^T
            if (cur && len > 1) {
                if (cur == len) cur--;
                i = (byte)line[cur - 1];
                line[cur - 1] = line[cur];
                line[cur++] = (char)i;
            }
            break;
        case 12:                            // ^L
            out("\033[H\033[2J");
            shlen = scol = 0;
            break;
        case 16: case K_UP: hist_move(-1); break;
        case 14: case K_DOWN: hist_move(1); break;
        case K_PGUP: hist_prefix(-1); break;
        case K_PGDN: hist_prefix(1); break;
        case 18: isearch(); break;
        case 9: complete(last == 9); break;
        default:
            if (c >= ' ' && c < 127) {
                char ch = (char)c;
                ins_text(&ch, 1);
            }
        }
        refresh();
    }
}

// ── Running commands ───────────────────────────────────────────────

byte jobpid[NJOBS];
word jobmod[NJOBS][MODS];
byte depth;

void unlink_all(word *mods) {
    byte i;
    for (i = 0; i < MODS; i++)
        if (mods[i]) m_unlink(mods[i]);
}

void job_done(byte pid, byte st) {
    byte i;
    for (i = 0; i < NJOBS && jobpid[i] != pid; i++) {}
    if (i < NJOBS) {
        unlink_all(jobmod[i]);
        jobpid[i] = 0;
    }
    put('-');
    outnum(pid, 3);
    out("\n");
    if (st) perr(st);
    ob_flush();
}

void handle_signal(byte pid) {
#ifdef _CMOC_VERSION_
    byte s = sigs[0];
    if (!sigs[1]) return;
    sigs[1] = 0;
    if (s == S_HUP) bye(0);
    // ^C or ^E while a command has not used the terminal since we did:
    // its Shell passes S$Abort on to it, so either one stops it.
    if (pid && (s == S_ABORT || s == S_INTRPT)) p_send(pid, S_ABORT);
#else
    (void)pid;
#endif
}

// Wait for pid to finish (0: any child), telling of the others that do.
byte wait_for(byte pid) {
    byte id, st, err;
    for (;;) {
        handle_signal(pid);
        err = p_wait(&id, &st);
        if (err) return pid ? err : 0;
        if (!id) continue;
        if (id == pid) return st;
        job_done(id, st);
        if (!pid) return st;
    }
}

char found[CWDMAX + 32];    // what resolve() found: a module name or a pathlist

// Where is command w?  0: leave it to the Shell; 1: a module, loaded, its
// name in found; 2: a procedure file, its pathlist in found.
byte resolve(const char *w, word *mod) {
    const char *d = pathv;
    byte i;
    int p;
    if (!npath || m_inmem(w) || m_inexec(w)) return 0;
    for (i = 0; i < npath; i++, d = path_next(d)) {
        strcpy(found, d);
        strcat(found, "/");
        strcat(found, w);
        *mod = m_load(found, found);
        if (*mod) return 1;
        if (oserr == E_PNNF) continue;
        p = f_open(found, READ_);
        if (p >= 0) {
            f_close((byte)p);
            return 2;
        }
    }
    return 0;
}

#define WORDBREAKS " \t!;&()<>#,\""

// Give the Shell a command line, with each command found on the path.
void run_external(char *s, byte bg) {
    static char cmd[CMDMAX + 1];
    word mods[MODS];
    word n = 0, wl, mod;
    byte nmod = 0, atcmd = 1, q = 0, r, pid, err;
    char w[32];
    memset(mods, 0, sizeof mods);
    while (*s && n < CMDMAX - 1) {
        if (atcmd && !q) {
            while (*s == ' ' && n < CMDMAX - 1) cmd[n++] = *s++;
            if (*s == '(') {
                cmd[n++] = *s++;
                continue;
            }
            wl = strcspn(s, WORDBREAKS);
            r = 0;
            if (wl && wl < sizeof w && !memchr(s, '/', wl)) {
                memcpy(w, s, wl);
                w[wl] = 0;
                r = resolve(w, &mod);
                if (r == 1 && nmod < MODS) mods[nmod++] = mod;
                else if (r == 1) { m_unlink(mod); r = 0; }
            }
            if (r && n + strlen(found) < CMDMAX - 1) {
                strcpy(cmd + n, found);
                n += strlen(found);
                s += wl;
            }
            atcmd = 0;
            continue;
        }
        if (*s == '"') q = !q;
        else if (!q && strchr("!;&(", *s)) atcmd = 1;
        cmd[n++] = *s++;
    }
    cmd[n] = 0;
    err = p_fork(cmd, n, &pid);
    if (err) {
        unlink_all(mods);
        perr(err);
        status = err;
        return;
    }
    if (bg) {
        for (r = 0; r < NJOBS && jobpid[r]; r++) {}
        if (r < NJOBS) {
            jobpid[r] = pid;
            memcpy(jobmod[r], mods, sizeof mods);
        }
        put('&');
        outnum(pid, 3);
        out("\n");
        ob_flush();
        status = 0;
        return;
    }
    status = wait_for(pid);
    unlink_all(mods);
    if (status && status != S_ABORT && status != S_INTRPT) perr(status);
}

void run_line(char *s);
void source(const char *name, byte quiet);
void bye(byte code);

void which(const char *w) {
    const char *d = pathv;
    byte i, n = (byte)strlen(w);
    int a = alias_find(w, n), p;
    char b[CWDMAX + 32];
    out(w);
    if (a >= 0) {
        out(": alias for ");
        out(path_next(alias_at(a)));
    } else if (builtin_find(w, n) >= 0) {
        out(": built in");
    } else if (strchr(w, '/')) {
        out(": a pathlist");
    } else if (m_inmem(w)) {
        out(": in memory");
    } else if (m_inexec(w)) {
        out(": in the execution directory");
    } else {
        for (i = 0; i < npath; i++, d = path_next(d)) {
            strcpy(b, d);
            strcat(b, "/");
            strcat(b, w);
            p = f_open(b, READ_);
            if (p >= 0) {
                f_close((byte)p);
                out(": ");
                out(b);
                break;
            }
        }
        if (i == npath) out(": not found (the Shell will try the data directory)");
    }
    out("\n");
}

void history_cmd(char *arg) {
    byte i, from = 0;
    if (!strcmp(arg, "-c")) {
        hcount = 0;
        hused = 0;
        hunsaved = 0;
        hist_rewrite();
        return;
    }
    if (*arg && number(arg) < hcount) from = hcount - (byte)number(arg);
    for (i = from; i < hcount; i++) {
        outnum(hbase + i, 4);
        out("  ");
        out(hist_get(i));
        out("\n");
    }
}

void cd(const char *dir, byte exec) {
    char old[CWDMAX];
    byte err;
    if (!strcmp(dir, "-")) dir = prevcwd[0] ? prevcwd : HOMEDIR;
    strcpy(old, cwd);
    err = f_chdir(dir, exec);
    status = err;
    if (err) {
        perr(err);
        return;
    }
    if (!exec) {
        cwd_find(cwd);
        strcpy(prevcwd, old);
    }
}

// Built-in command i, with the rest of its command (after its name).
void show_settings(void) {
    out("PATH=");
    show_path(':');
    out("PROMPT=\"");
    out(promptfmt);
    out("\"\nCOLS=");
    outnum(cols, 1);
    out("\n");
}

// NAME=value: the settings.  Returns 0 if s is not one.
byte assign(char *s) {
    char *v = strchr(s, '=');
    word n;
    if (!v || v == s) return 0;
    n = (word)(v - s);
    v = unquote(v + 1);
    if (n == 4 && starts(s, "PATH", 4)) path_set(v);
    else if (n == 6 && starts(s, "PROMPT", 6)) strncpy(promptfmt, v, sizeof promptfmt - 1);
    else if (n == 4 && starts(s, "COLS", 4)) {
        if (number(v) >= 40 && number(v) < MAXCOLS) cols = (byte)number(v);
    } else {
        outn(s, (byte)n);
        out(": tsh has PATH, PROMPT and COLS\n");
    }
    ob_flush();
    return 1;
}

// Built-in command i, with the rest of its command (after its name).
void builtin(byte i, char *rest, byte bg) {
    char *argv[8], *text;
    byte argc, k, id, st;
    int a;
    switch (i) {
    case 15: case 16: case 17: case 18: case 19: case 20:  // x -x p -p t -t
        if (i < 17) opt_x = i == 15;
        else if (i < 19) opt_nop = i == 18;
        else opt_t = i == 19;
        if (*rest) run_line(rest);         // more may follow, as in the Shell
        return;
    case 5:                                 // alias [name [text]]
        rest = skipsp(rest);
        if (!*rest) {
            for (text = aliasv; text < aliasv + aliaslen; text = (char *)path_next(path_next(text)))
                alias_show(text);
            ob_flush();
            return;
        }
        text = rest + strcspn(rest, " \t");
        if (*text) *text++ = 0;
        text = unquote(text);
        if (*text) alias_set(rest, text);
        else if ((a = alias_find(rest, (byte)strlen(rest))) >= 0) alias_show(alias_at(a));
        else { out(rest); out(": no such alias\n"); }
        ob_flush();
        return;
    case 21:                                // ex
        rest = skipsp(rest);
        if (*rest) run_external(rest, 0);
        bye(status);
    }
    argc = split(rest, argv, 8);
    switch (i) {
    case 0: case 1:                         // cd chd
        cd(argc ? argv[0] : HOMEDIR, 0);
        break;
    case 2:                                 // chx
        if (argc) cd(argv[0], 1);
        break;
    case 3:                                 // path
        if (!argc) show_path(' ');
        for (k = 0; k < argc; k++) {
            if (argv[k][0] == '+') path_add(argv[k] + 1, strlen(argv[k] + 1));
            else if (!strcmp(argv[k], "-")) path_clear();
            else if (argv[k][0] == '-') path_del(argv[k] + 1);
            else {
                if (!k) path_clear();
                path_add(argv[k], strlen(argv[k]));
            }
        }
        break;
    case 4:                                 // history
        history_cmd(argc ? argv[0] : "");
        break;
    case 6:                                 // unalias
        for (k = 0; k < argc; k++) alias_del(argv[k]);
        break;
    case 7: case 8:                         // source .
        if (argc) source(argv[0], 0);
        break;
    case 9:                                 // which
        for (k = 0; k < argc; k++) which(argv[k]);
        break;
    case 10:                                // set
        show_settings();
        break;
    case 11: case 12:                       // w wait: for the background
        for (;;) {
            for (k = 0; k < NJOBS && !jobpid[k]; k++) {}
            if (k == NJOBS) break;
            if (p_wait(&id, &st)) {
                memset(jobpid, 0, NJOBS);
                break;
            }
            if (id) job_done(id, st);
            else handle_signal(0);
        }
        break;
    case 13:                                // kill
        for (k = 0; k < argc; k++)
            if ((status = p_send((byte)number(argv[k]), 0)) != 0) perr(status);
        break;
    case 14:                                // setpr
        if (argc == 2 && (status = p_setpr((byte)number(argv[0]), (byte)number(argv[1]))) != 0)
            perr(status);
        break;
    case 22:                                // exit
        bye(argc ? (byte)number(argv[0]) : 0);
    }
    (void)bg;
    ob_flush();
}

// One command: an alias, a built-in command, or one for the Shell.
void run_command(char *s, byte bg) {
    static char xbuf[DEPTH][LINEMAX + 4];
    word wl, n;
    int a;
    byte k;
    char *rest;
    s = skipsp(s);
    n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = 0;
    if (!n) return;
    wl = strcspn(s, WORDBREAKS);
    rest = skipsp(s + wl);
    a = wl ? alias_find(s, (byte)wl) : -1;
    for (k = 0; a >= 0 && k < depth; k++)
        if (abusy[k] == a) a = -1;
    if (a >= 0 && depth < DEPTH) {
        char *x = xbuf[depth];
        strncpy(x, path_next(alias_at(a)), LINEMAX);
        x[LINEMAX] = 0;
        if (*rest && strlen(x) + strlen(rest) + 1 < LINEMAX) {
            strcat(x, " ");
            strcat(x, rest);
        }
        if (bg && strlen(x) + 2 < LINEMAX + 3) strcat(x, " &");
        abusy[depth++] = a;
        run_line(x);
        depth--;
        return;
    }
    if (wl > 1 && memchr(s, '=', wl) && !starts(s, "i=", 2) && assign(s)) return;
    if (wl >= 2 && starts(s, "i=", 2)) {
        out("tsh: i= is for the Shell\n");
        ob_flush();
        return;
    }
    if ((a = builtin_find(s, wl)) >= 0) {
        builtin((byte)a, rest, bg);
        return;
    }
    run_external(s, bg);
}

// A command line: commands separated by ; and &, outside quotes and ( ).
void run_line(char *s) {
    char *e, sep;
    byte q, par;
    for (;;) {
        s = skipsp(s);
        if (!*s || *s == '*') return;       // * starts a comment
        q = par = 0;
        for (e = s; *e; e++) {
            if (*e == '"') q = !q;
            else if (q) continue;
            else if (*e == '(') par++;
            else if (*e == ')') { if (par) par--; }
            else if (!par && (*e == ';' || *e == '&')) break;
        }
        sep = *e;
        *e = 0;
        run_command(s, sep == '&');
        *e = sep;
        if (!sep || (opt_x && status)) return;
        s = e + 1;
    }
}

// Run the lines of a file, or of the standard input (path 0).
char sbuf[DEPTH][LINEMAX + 2];

void run_stream(byte path) {
    word n;
    char *b;
    if (depth >= DEPTH) return;
    abusy[depth] = -1;
    b = sbuf[depth++];
    while ((n = f_read(path, b, LINEMAX + 1, 1)) > 0) {
        while (n && (b[n - 1] == CR || b[n - 1] == LF)) n--;
        b[n] = 0;
        if (opt_t) { out(b); out("\n"); ob_flush(); }
        run_line(b);
        if (opt_x && status) break;
    }
    depth--;
}

void source(const char *name, byte quiet) {
    int p = f_open(name, READ_);
    if (p < 0) {
        if (!quiet) { perr((byte)oserr); status = (byte)oserr; }
        return;
    }
    run_stream((byte)p);
    f_close((byte)p);
}

void bye(byte code) {
    if (interactive) hist_flush();
    ob_flush();
    tty_done();
    p_exit(code);
}

int main(int argc, char **argv) {
#ifndef _CMOC_VERSION_
    char *e;
#endif
    outpath = 1;                // (no initialized data: see the Makefile)
    cols = 80;
    hbase = 1;
    pending = -1;
    plat_init();
    strcpy(promptfmt, "OS9:");
    if (argc > 1) {
        source(argv[1], 0);
        bye(status);
    }
    interactive = tty_init();
    if (!interactive) {
        run_stream(0);
        bye(status);
    }
#ifndef _CMOC_VERSION_
    if ((e = getenv("TSH_COLS"))) cols = (byte)number(e);
#endif
    cwd_find(cwd);
    hist_load();
    source(RCFILE, 1);
    for (;;) {
        make_prompt();
        tty_edit(1);
        if (!edit_line()) break;
        tty_edit(0);
        if (len && hist_add(line) && ++hunsaved >= HISTSAVE) hist_flush();
        run_line(line);
        if (opt_x && status) break;
    }
    bye(status);
    return 0;
}
