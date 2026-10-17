// zm.c -- a Z-machine for NitrOS-9 Level 2 on the TFR/911H.
//
// Plays version 3 story files: most Infocom games of 1980-1987 (Zork I-III,
// Hitchhiker's Guide, Planetfall, Enchanter, Deadline, ...), and Inform
// games compiled with -v3.
//
//   zm [-w cols] [-h rows] [-m kbytes] [-d] story[.z3]
//
// A story named without a path is looked for here, then in /DD/GAMES.
//
//   -w, -h  the terminal's size (default 80x24)
//   -m      use at most this much memory for the story
//   -d      dumb terminal: no status line, no escape sequences, no [MORE]
//
// Memory: dynamic memory (the part the game can change) stays in RAM.  If
// the whole story fits, it is all loaded; otherwise the rest is read from
// the story file in 512-byte pages as needed, into as many page slots as
// the process can get (by growing its data area with F$Mem), least
// recently used out first.
//
// Screen: a status line in reverse video on the top row, with the text
// scrolling beneath it (an ANSI scroll region), word-wrapped, pausing with
// [MORE] when a screenful goes by without input.
//
// The same source builds on a Unix host (cc -o zm zm.c), for testing.
//
// Build for OS-9:  cmoc --os9 --function-stack=0 --add-os9-stack-space=2048 -o zm zm.c

#ifdef _CMOC_VERSION_
#include <cmoc.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <termios.h>
#include <sys/stat.h>
#endif

typedef unsigned char byte;
typedef unsigned short zword;
typedef short zsword;

#define PAGE 512
#define MAXSLOTS 120        // page slots; 120 * 512 = 60K
#define STACK 1024          // words of evaluation stack and locals
#define CALLS 96            // routine call depth

void fatal(const char *msg);
void finish(byte code);

// ── Platform: NitrOS-9 ─────────────────────────────────────────────

#ifdef _CMOC_VERSION_

byte plat_sig;              // set by the signal intercept (^C, ^E)
byte story_path;
byte key_path;              // our own path to /term, for [MORE] keys

// OS-9 path names end with a CR.
void os9name(char *dst, const char *src) {
    byte i = 0;
    while (*src && i < 60) dst[i++] = *src++;
    dst[i] = 13;
}

byte os9_open(const char *name, byte mode) {
    char n[62];
    byte path = 255;
    os9name(n, name);
    asm {
        lda     :mode
        leax    :n
        os9     $84             ; I$Open
        bcs     @err
        sta     :path
@err
    }
    return path;
}

void os9_delete(char *n) {
    asm {
        ldx     :n
        os9     $87             ; I$Delete
    }
}

byte os9_create(const char *name) {
    char n[62];
    byte path = 255, err = 0;
    os9name(n, name);
    asm {
        lda     #2              ; WRITE.
        ldb     #$0B            ; owner read+write, public read
        leax    :n
        os9     $83             ; I$Create
        bcs     @err
        sta     :path
        clrb
@err
        stb     :err
    }
    if (err == 218) {           // E$CEF: it exists, so replace it
        os9_delete(n);
        asm {
            lda     #2
            ldb     #$0B
            leax    :n
            os9     $83         ; I$Create
            bcs     @err2
            sta     :path
@err2
        }
    }
    return path;
}

void os9_close(byte path) {
    asm {
        lda     :path
        os9     $8F             ; I$Close
    }
}

// Read (or with line set, I$ReadLn) up to n bytes; returns the count, or
// 0xFFFF on an error (end of file included).
zword os9_read(byte path, byte *buf, zword n, byte line) {
    zword got = 0;
    byte err = 0;
    if (line) {
        asm {
            lda     :path
            ldx     :buf
            pshs    y
            ldy     :n
            os9     $8B         ; I$ReadLn
            bcc     @ok
            stb     :err
            ldy     #0
@ok
            sty     :got
            puls    y
        }
    } else {
        asm {
            lda     :path
            ldx     :buf
            pshs    y
            ldy     :n
            os9     $89         ; I$Read
            bcc     @ok2
            stb     :err
            ldy     #0
@ok2
            sty     :got
            puls    y
        }
    }
    return err ? 0xFFFF : got;
}

byte os9_write(byte path, const void *buf, zword n) {
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
    return err;
}

void os9_seek(byte path, zword msw, zword lsw) {
    asm {
        lda     :path
        ldx     :msw
        pshs    u
        ldu     :lsw
        os9     $88             ; I$Seek
        puls    u
    }
}

// F$Mem: ask for a data area of size bytes (0: no change).  Returns the
// new size, or 0 if refused, and sets *top to its upper bound.
zword os9_mem(zword size, zword *top) {
    zword got = 0, t = 0;
    asm {
        ldd     :size
        pshs    y
        os9     $07             ; F$Mem
        bcs     @err
        std     :got
        sty     :t
@err
        puls    y
    }
    *top = t;
    return got;
}

asm __norts__ void intercept(void) {
    asm {
        stb     ,u
        rti
    }
}

void plat_init(void) {
    byte *flag = &plat_sig;
    void (*handler)(void) = intercept;
    asm {
        ldx     :handler
        pshs    u
        ldu     :flag
        os9     $09             ; F$Icpt
        puls    u
    }
}

void plat_exit(byte code) {
    asm {
        ldb     :code
        os9     $06             ; F$Exit
    }
}

byte plat_open_story(const char *name) {
    story_path = os9_open(name, 1);
    return story_path != 255;
}

// The story's length in 512-byte pages, from SS.Size.
zword plat_story_pages(void) {
    byte path = story_path;
    zword msw = 0, lsw = 0;
    asm {
        lda     :path
        ldb     #2              ; SS.Size
        pshs    u
        os9     $8D             ; I$GetStt
        tfr     u,d
        puls    u
        bcs     @err
        stx     :msw
        std     :lsw
@err
    }
    return (msw << 7) + (lsw >> 9) + ((lsw & 511) != 0);
}

void plat_read_page(byte page, byte *buf) {
    zword got, i;
    os9_seek(story_path, page >> 7, (zword)(page & 127) << 9);
    got = os9_read(story_path, buf, PAGE, 0);
    if (got == 0xFFFF) got = 0;
    for (i = got; i < PAGE; i++) buf[i] = 0;
}

void plat_write(const char *p, zword n) {
    os9_write(1, p, n);
}

// A line from the keyboard, without its CR: its length, or 0xFFFF.
zword plat_readline(char *buf, zword max) {
    zword n = os9_read(0, (byte *)buf, max + 1, 1);
    if (n == 0xFFFF) return n;
    if (n && buf[n - 1] == 13) n--;
    if (n > max) n = max;
    return n;
}

void plat_getkey(void) {
    byte opts[32];
    byte path, ch;
    if (!key_path) {
        key_path = os9_open("/term", 3);
        if (key_path == 255) return;
        path = key_path;
        asm {
            lda     :path
            ldb     #0          ; SS.Opt
            leax    :opts
            os9     $8D         ; I$GetStt
        }
        opts[4] = 0;            // PD.EKO: no echo
        opts[7] = 0;            // PD.PAU: no page pause
        opts[12] = 0;           // PD.EOF
        asm {
            lda     :path
            ldb     #0
            leax    :opts
            os9     $8E         ; I$SetStt
        }
    }
    if (key_path != 255) os9_read(key_path, &ch, 1, 0);
}

int plat_create(const char *name) {
    byte p = os9_create(name);
    return p == 255 ? -1 : p;
}

int plat_openr(const char *name) {
    byte p = os9_open(name, 1);
    return p == 255 ? -1 : p;
}

byte plat_fwrite(int f, const void *p, zword n) {
    return os9_write((byte)f, p, n) == 0;
}

byte plat_fread(int f, void *p, zword n) {
    return os9_read((byte)f, (byte *)p, n, 0) == n;
}

void plat_close(int f) {
    os9_close((byte)f);
}

zword plat_seed(void) {
    byte t[6];
    asm {
        leax    :t
        os9     $15             ; F$Time
    }
    return ((zword)t[4] << 8 | t[5]) ^ ((zword)t[3] << 11) ^ ((zword)t[2] << 3);
}

// Grow the data area by as much as we can, up to *size bytes.
byte *plat_alloc(zword *size) {
    zword cur, top, ntop, want;
    cur = os9_mem(0, &top);
    if (!cur) return 0;
    want = *size;
    if (want > 0xF000 - top) want = 0xF000 - top;
    for (; want >= 4096; want -= PAGE) {
        if (os9_mem(cur + want, &ntop)) {
            *size = ntop - top;
            return (byte *)top;
        }
    }
    return 0;
}

// The Lurking Horror's beeps, on the TFR/911H's SN76489 at $FF41.
void plat_beep(byte high) {
    zword n = high ? 112 : 447;     // about 1000 Hz and 250 Hz
    byte *snd = (byte *)0xFF41;
    *snd = (byte)(0x80 | (n & 15));
    *snd = (byte)(n >> 4);
    *snd = 0x90;                    // voice 0 loudest
    asm {
        ldx     #6
        os9     $0A             ; F$Sleep: 1/10 second
    }
    *snd = 0x9F;                    // off
}

// ── Platform: Unix (for testing) ───────────────────────────────────

#else

volatile byte plat_sig;
int story_fd = -1;

void on_signal(int s) { (void)s; plat_sig = 1; }

void plat_init(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, 0);      // no SA_RESTART: input returns
}

long page_reads;

void plat_exit(byte code) {
    if (getenv("ZM_STATS")) fprintf(stderr, "zm: %ld page reads\n", page_reads);
    exit(code);
}

byte plat_open_story(const char *name) {
    story_fd = open(name, O_RDONLY);
    return story_fd >= 0;
}

zword plat_story_pages(void) {
    struct stat st;
    if (fstat(story_fd, &st)) return 0;
    return (zword)((st.st_size + PAGE - 1) / PAGE);
}

void plat_read_page(byte page, byte *buf) {
    ssize_t got = pread(story_fd, buf, PAGE, (off_t)page * PAGE);
    if (got < 0) got = 0;
    memset(buf + got, 0, PAGE - got);
    page_reads++;
}

void plat_write(const char *p, zword n) {
    if (write(1, p, n) < 0) exit(1);
}

zword plat_readline(char *buf, zword max) {
    char line[512];
    size_t n;
    if (!fgets(line, sizeof line, stdin)) return 0xFFFF;
    n = strcspn(line, "\r\n");
    if (!isatty(0)) {               // show piped input, as if typed
        plat_write(line, (zword)n);
        plat_write("\n", 1);
    }
    if (n > max) n = max;
    memcpy(buf, line, n);
    return (zword)n;
}

void plat_getkey(void) {
    struct termios t, raw;
    char ch;
    if (!isatty(0)) return;
    tcgetattr(0, &t);
    raw = t;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &raw);
    if (read(0, &ch, 1) < 0) plat_sig = 1;
    tcsetattr(0, TCSANOW, &t);
}

int plat_create(const char *name) { return open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644); }
int plat_openr(const char *name) { return open(name, O_RDONLY); }
byte plat_fwrite(int f, const void *p, zword n) { return write(f, p, n) == n; }
byte plat_fread(int f, void *p, zword n) { return read(f, p, n) == n; }
void plat_close(int f) { close(f); }
zword plat_seed(void) { return (zword)time(0); }

byte *plat_alloc(zword *size) { return malloc(*size); }

void plat_beep(byte high) { (void)high; plat_write("\a", 1); }

#endif

// ── Story memory ───────────────────────────────────────────────────

byte *mem;                  // story bytes 0 .. rsize-1, always in RAM
zword rsize;
zword dyn_end;              // the static memory base: the game can write below it
zword npages;               // the story's length in pages
byte *pmap[256];            // each page's bytes, or 0 if not loaded
byte pslot[256];            // each loaded page's slot + 1 (0 if resident)
byte *slots;                // page slots for the rest of the story
byte nslots, nfilled;
byte slot_page[MAXSLOTS];
zword slot_used[MAXSLOTS];
zword tick;

// The program counter: pcp points into pcbase, the bytes of story address
// pcb_hi:pcb_lo (a 17-bit address), valid up to pcend.
byte *pcp, *pcend, *pcbase;
byte pcb_hi;
zword pcb_lo;
byte pc_hi;                 // get_pc() leaves the PC here
zword pc_lo;

byte *slot_ptr(byte i) { return slots + ((zword)i << 9); }

// Load a page into the least recently used slot (never the PC's).
byte *fault(byte pg) {
    byte i, v = 0;
    zword age, best = 0;
    byte *p;
    if (pg >= npages) fatal("address beyond the story");
    if (nfilled < nslots) {
        v = nfilled++;
    } else {
        for (i = 0; i < nslots; i++) {
            if (slot_ptr(i) == pcbase) continue;
            age = tick - slot_used[i];
            if (age >= best) {
                best = age;
                v = i;
            }
        }
        pmap[slot_page[v]] = 0;
        pslot[slot_page[v]] = 0;
    }
    p = slot_ptr(v);
    plat_read_page(pg, p);
    pmap[pg] = p;
    pslot[pg] = v + 1;
    slot_page[v] = pg;
    slot_used[v] = ++tick;
    return p;
}

byte *page_ptr(byte pg) {
    byte s;
    byte *p = pmap[pg];
    if (!p) return fault(pg);
    s = pslot[pg];
    if (s) slot_used[s - 1] = ++tick;
    return p;
}

byte rb(zword a) {
    byte *p;
    if (a < rsize) return mem[a];
    p = pmap[a >> 9];
    if (!p) p = fault((byte)(a >> 9));
    return p[a & 511];
}

zword rw(zword a) {
    zword w;
    if (a < rsize - 1) return ((zword)mem[a] << 8) | mem[a + 1];
    w = (zword)rb(a) << 8;
    return w | rb(a + 1);
}

byte rb17(byte hi, zword lo) {
    byte pg;
    byte *p;
    if (!hi) return rb(lo);
    pg = (byte)(0x80 | (lo >> 9));
    p = pmap[pg];
    if (!p) p = fault(pg);
    return p[lo & 511];
}

void wb(zword a, byte v) {
    if (a >= dyn_end) fatal("write to static memory");
    mem[a] = v;
}

void ww(zword a, zword v) {
    if (a + 1 >= dyn_end) fatal("write to static memory");
    mem[a] = (byte)(v >> 8);
    mem[a + 1] = (byte)v;
}

void set_pc(byte hi, zword lo) {
    byte pg;
    if (!hi && lo < rsize) {
        pcbase = mem;
        pcb_hi = 0;
        pcb_lo = 0;
        pcp = mem + lo;
        pcend = mem + rsize;
        return;
    }
    if (hi > 1) fatal("jump out of range");
    pg = (byte)((hi << 7) | (lo >> 9));
    pcbase = page_ptr(pg);
    pcb_hi = hi;
    pcb_lo = lo & 0xFE00;
    pcp = pcbase + (lo & 511);
    pcend = pcbase + PAGE;
}

void get_pc(void) {
    zword off = (zword)(pcp - pcbase);
    pc_lo = pcb_lo + off;
    pc_hi = pcb_hi + (pc_lo < pcb_lo);
}

byte pc_cross(void) {
    get_pc();
    set_pc(pc_hi, pc_lo);
    return *pcp++;
}

#define FETCH() (pcp < pcend ? *pcp++ : pc_cross())

zword fetchw(void) {
    zword w = (zword)FETCH() << 8;
    return w | FETCH();
}

void pc_add(zsword d) {
    zword off = (zword)(pcp - pcbase);
    zword n = off + (zword)d;
    zword lo;
    byte hi;
    if (d >= 0 ? (n >= off && n < (zword)(pcend - pcbase)) : n <= off) {
        pcp = pcbase + n;
        return;
    }
    get_pc();
    lo = pc_lo + (zword)d;
    hi = pc_hi;
    if (d >= 0) {
        if (lo < pc_lo) hi++;
    } else if (lo > pc_lo) {
        hi--;
    }
    set_pc(hi, lo);
}

// ── Header ─────────────────────────────────────────────────────────

zword h_dict, h_objects, h_globals, h_abbrevs;
byte dumb;

void setup_header(void) {
    byte f1 = mem[1];
    f1 &= ~0x70;                    // fixed-pitch font, status line available
    if (dumb) f1 |= 0x10;           // no status line
    else f1 |= 0x20;                // screen splitting available
    mem[1] = f1;
    mem[0x32] = 1;                  // Standard 1.0
    mem[0x33] = 0;
}

// ── Output ─────────────────────────────────────────────────────────

byte cols = 80, rows = 24;
byte col, row, lines;       // cursor column, row (lower window), lines since input
byte upper, window;         // upper window height, and the current window
byte screen_on = 1;
char obuf[256];
zword olen;
char wbuf[128];             // the word being printed
byte wlen;
char *cap;                  // capture output here (for the status line)
byte caplen, capmax;
zword s3tab[16], s3len[16]; // output stream 3 tables
byte s3depth;

void oflush(void) {
    if (olen) {
        plat_write(obuf, olen);
        olen = 0;
    }
}

void ochar(char c) {
    obuf[olen++] = c;
    if (olen == sizeof obuf) oflush();
}

void ostr(const char *s) {
    while (*s) ochar(*s++);
}

char *fmtnum(char *p, zsword n) {
    char d[6];
    byte i = 0;
    zword u = (zword)n;
    if (n < 0) {
        *p++ = '-';
        u = (zword)(0 - u);
    }
    do {
        d[i++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (i) *p++ = d[--i];
    *p = 0;
    return p;
}

void onum(zsword n) {
    char b[8];
    fmtnum(b, n);
    ostr(b);
}

// ESC 7.  (Not "\0337": CMOC reads that as one octal escape.)
void save_cursor(void) {
    ochar(27);
    ochar('7');
}

void goto_row(byte r) {
    ostr("\033[");
    onum(r);
    ostr(";1H");
}

void more(void) {
    ostr("[MORE]");
    oflush();
    plat_getkey();
    ostr("\r      \r");
    lines = 0;
}

void newline(void) {
    if (dumb) {
        ochar('\n');
    } else {
        ochar('\r');
        ochar('\n');
        if (row < rows) row++;
        if (++lines >= rows - upper - 2) more();
    }
    col = 0;
}

void wflush(void) {
    byte i;
    if (!wlen) return;
    if (col && col + wlen > cols - 1) newline();
    for (i = 0; i < wlen; i++) ochar(wbuf[i]);
    col += wlen;
    wlen = 0;
}

void screen_putc(byte c) {
    if (window) {                   // the upper window: no wrapping
        if (c == 13) {
            ochar('\r');
            ochar('\n');
        } else {
            ochar(c);
        }
        return;
    }
    if (c == 13) {
        wflush();
        newline();
    } else if (c == ' ') {
        wflush();
        if (col >= cols - 1) {
            newline();
        } else {
            ochar(' ');
            col++;
        }
    } else {
        wbuf[wlen++] = c;
        if (wlen >= cols - 1) wflush();
    }
}

void zputc(zword c) {
    byte d;
    if (s3depth) {
        d = s3depth - 1;
        wb(s3tab[d] + 2 + s3len[d], (byte)c);
        s3len[d]++;
        return;
    }
    if (cap) {
        if (caplen < capmax) cap[caplen++] = c == 13 ? ' ' : (char)c;
        return;
    }
    if (!screen_on) return;
    if (c == 9) c = ' ';
    if (c != 13 && (c < 32 || c > 126)) c = '?';
    screen_putc((byte)c);
}

void zputs(const char *s) {
    while (*s) zputc((byte)*s++);
}

void screen_init(void) {
    if (dumb) return;
    ostr("\033[2J\033[2r");         // clear; scroll below the status line
    goto_row(2);
    row = 2;
}

void screen_done(void) {
    wflush();
    if (!dumb) {
        if (window) ostr("\0338");
        ostr("\033[r");             // whole-screen scrolling again
        goto_row(rows);
        ostr("\r\n");
    }
    oflush();
}

void split_window(zword n) {
    byte i;
    if (dumb) return;
    wflush();
    if (n > rows - 4) n = rows - 4;
    upper = (byte)n;
    save_cursor();                  // and set the scroll region
    ostr("\033[");
    onum(2 + upper);
    ostr("r");
    for (i = 0; i < upper; i++) {   // version 3 clears the upper window
        goto_row(2 + i);
        ostr("\033[K");
    }
    ostr("\0338");
    if (row < 2 + upper) {
        row = 2 + upper;
        goto_row(row);
    }
}

void set_window(zword w) {
    if (dumb) {
        window = 0;
        return;
    }
    wflush();
    if (w && !window) {
        save_cursor();
        goto_row(2);
    } else if (!w && window) {
        ostr("\0338");
    }
    window = w != 0;
}

// ── Text ───────────────────────────────────────────────────────────

const char a2[] = " \r0123456789.,!?_#'\"/\\-:()";  // z-chars 6-31 of A2

byte zs_pc;                 // decode from the PC, or else from zs_hi:zs_lo
byte zs_hi;
zword zs_lo;

zword zs_next(void) {
    zword w;
    if (zs_pc) return fetchw();
    w = (zword)rb17(zs_hi, zs_lo) << 8;
    if (!++zs_lo) zs_hi++;
    w |= rb17(zs_hi, zs_lo);
    if (!++zs_lo) zs_hi++;
    return w;
}

void decode(void);

void print_abbrev(byte idx) {
    byte spc = zs_pc, shi = zs_hi;
    zword slo = zs_lo;
    zword wa = rw(h_abbrevs + ((zword)idx << 1));
    zs_pc = 0;
    zs_hi = (byte)(wa >> 15);
    zs_lo = wa << 1;
    decode();
    zs_pc = spc;
    zs_hi = shi;
    zs_lo = slo;
}

void decode(void) {
    zword w;
    byte i, c, alpha = 0, abbr = 0, esc = 0, hi = 0;
    do {
        w = zs_next();
        for (i = 0; i < 3; i++) {
            if (i == 0) c = (byte)(w >> 10) & 31;
            else if (i == 1) c = (byte)(w >> 5) & 31;
            else c = (byte)w & 31;
            if (abbr) {
                print_abbrev((byte)(((abbr - 1) << 5) | c));
                abbr = 0;
            } else if (esc == 1) {
                hi = c;
                esc = 2;
            } else if (esc == 2) {
                zputc(((zword)hi << 5) | c);
                esc = 0;
            } else if (c == 0) {
                zputc(' ');
            } else if (c < 4) {
                abbr = c;
            } else if (c < 6) {
                alpha = c - 3;      // a shift: just for the next z-char
                continue;
            } else if (alpha == 0) {
                zputc(c - 6 + 'a');
            } else if (alpha == 1) {
                zputc(c - 6 + 'A');
            } else if (c == 6) {
                esc = 1;
            } else {
                zputc((byte)a2[c - 6]);
            }
            alpha = 0;
        }
    } while (!(w & 0x8000));
}

void print_at(byte hi, zword lo) {
    zs_pc = 0;
    zs_hi = hi;
    zs_lo = lo;
    decode();
}

void print_num(zword n) {
    char b[8];
    fmtnum(b, (zsword)n);
    zputs(b);
}

// ── Variables and routines ─────────────────────────────────────────

zword stk[STACK];
zword sp, bp;               // stack pointer; the current routine's locals
byte nloc;

struct frame {
    zword lo, bp;
    byte hi, store, nloc;
} calls[CALLS];
byte ncalls;

zword ops[4];
byte nops;

zword var_read(byte v) {
    zword a;
    if (v == 0) {
        if (!sp) fatal("stack underflow");
        return stk[--sp];
    }
    if (v < 16) return stk[bp + v - 1];
    a = h_globals + ((zword)(v - 16) << 1);
    return ((zword)mem[a] << 8) | mem[a + 1];
}

void var_write(byte v, zword x) {
    zword a;
    if (v == 0) {
        if (sp == STACK) fatal("stack overflow");
        stk[sp++] = x;
    } else if (v < 16) {
        stk[bp + v - 1] = x;
    } else {
        a = h_globals + ((zword)(v - 16) << 1);
        mem[a] = (byte)(x >> 8);
        mem[a + 1] = (byte)x;
    }
}

// The indirect forms (inc, dec, load, store, pull ...) use the top of the
// stack in place.
zword var_peek(byte v) {
    if (v == 0) {
        if (!sp) fatal("stack underflow");
        return stk[sp - 1];
    }
    return var_read(v);
}

void var_poke(byte v, zword x) {
    if (v == 0) {
        if (!sp) fatal("stack underflow");
        stk[sp - 1] = x;
    } else {
        var_write(v, x);
    }
}

void store(zword x) {
    var_write(FETCH(), x);
}

void zret(zword x) {
    struct frame *f;
    if (!ncalls) fatal("return from the main routine");
    f = &calls[--ncalls];
    sp = bp;
    bp = f->bp;
    nloc = f->nloc;
    set_pc(f->hi, f->lo);
    var_write(f->store, x);
}

void branch(byte cond) {
    byte b = FETCH();
    zsword off;
    if (b & 0x40) {
        off = b & 0x3F;
    } else {
        off = (zsword)(((zword)(b & 0x3F) << 8) | FETCH());
        if (off & 0x2000) off -= 0x4000;
    }
    if (!(b & 0x80) == !cond) {
        if (off == 0 || off == 1) zret((zword)off);
        else pc_add(off - 2);
    }
}

void zcall(byte storevar) {
    byte n, i;
    zword w, packed = ops[0];
    struct frame *f;
    if (!packed) {
        var_write(storevar, 0);
        return;
    }
    if (plat_sig) finish(1);
    if (ncalls == CALLS) fatal("too many nested calls");
    get_pc();
    f = &calls[ncalls++];
    f->hi = pc_hi;
    f->lo = pc_lo;
    f->bp = bp;
    f->store = storevar;
    f->nloc = nloc;
    bp = sp;
    set_pc((byte)(packed >> 15), packed << 1);
    n = FETCH();
    if (n > 15 || sp + n > STACK) fatal("bad routine");
    for (i = 0; i < n; i++) {
        w = fetchw();
        stk[sp++] = i + 1 < nops ? ops[i + 1] : w;
    }
    nloc = n;
}

// ── Objects ────────────────────────────────────────────────────────

zword obj_addr(zword o) {
    return h_objects + 62 + ((o - 1) << 3) + (o - 1);
}

byte obj_parent(zword o) { return o ? rb(obj_addr(o) + 4) : 0; }
byte obj_sibling(zword o) { return o ? rb(obj_addr(o) + 5) : 0; }
byte obj_child(zword o) { return o ? rb(obj_addr(o) + 6) : 0; }

byte attr_test(zword o, zword a) {
    if (!o || a > 31) return 0;
    return (rb(obj_addr(o) + (a >> 3)) & (0x80 >> (a & 7))) != 0;
}

void attr_set(zword o, zword a, byte on) {
    zword p;
    byte m;
    if (!o || a > 31) return;
    p = obj_addr(o) + (a >> 3);
    m = (byte)(0x80 >> (a & 7));
    wb(p, on ? rb(p) | m : rb(p) & ~m);
}

void remove_obj(zword o) {
    zword oa, pa, x, xa;
    byte p;
    if (!o) return;
    oa = obj_addr(o);
    p = rb(oa + 4);
    if (!p) return;
    pa = obj_addr(p);
    if (rb(pa + 6) == o) {
        wb(pa + 6, rb(oa + 5));
    } else {
        x = rb(pa + 6);
        while (x) {
            xa = obj_addr(x);
            if (rb(xa + 5) == o) {
                wb(xa + 5, rb(oa + 5));
                break;
            }
            x = rb(xa + 5);
        }
    }
    wb(oa + 4, 0);
    wb(oa + 5, 0);
}

void insert_obj(zword o, zword d) {
    zword oa, da;
    if (!o || !d) return;
    remove_obj(o);
    oa = obj_addr(o);
    da = obj_addr(d);
    wb(oa + 5, rb(da + 6));
    wb(da + 6, (byte)o);
    wb(oa + 4, (byte)d);
}

zword prop_table(zword o) { return rw(obj_addr(o) + 7); }

zword first_prop(zword o) {
    zword t = prop_table(o);
    return t + 1 + ((zword)rb(t) << 1);
}

// The address of property p's size byte, or 0.
zword find_prop(zword o, zword p) {
    zword a;
    byte sb, n;
    if (!o) return 0;
    a = first_prop(o);
    for (;;) {
        sb = rb(a);
        n = sb & 31;
        if (n == p) return sb ? a : 0;
        if (n < p) return 0;        // properties are in descending order
        a += (sb >> 5) + 2;
    }
}

zword get_prop(zword o, zword p) {
    zword a = find_prop(o, p);
    if (!a) return rw(h_objects + ((p - 1) << 1));
    if (rb(a) >> 5) return rw(a + 1);
    return rb(a + 1);
}

void put_prop(zword o, zword p, zword v) {
    zword a = find_prop(o, p);
    if (!a) fatal("put_prop: no such property");
    if (rb(a) >> 5) ww(a + 1, v);
    else wb(a + 1, (byte)v);
}

zword next_prop(zword o, zword p) {
    zword a;
    if (!o) return 0;
    if (!p) {
        a = first_prop(o);
    } else {
        a = find_prop(o, p);
        if (!a) fatal("get_next_prop: no such property");
        a += (rb(a) >> 5) + 2;
    }
    return rb(a) & 31;
}

zword prop_len(zword a) {
    if (!a) return 0;
    return (rb(a - 1) >> 5) + 1;
}

void print_obj(zword o) {
    zword t;
    if (!o) return;
    t = prop_table(o);
    if (rb(t)) print_at(0, t + 1);
}

// ── Input ──────────────────────────────────────────────────────────

void show_status(void) {
    char name[100], right[40];
    char *p;
    zsword a, b;
    byte len, rlen, i;
    if (dumb || window) return;
    wflush();
    cap = name;
    caplen = 0;
    capmax = sizeof name - 1;
    print_obj(var_read(16));
    cap = 0;
    a = (zsword)var_read(17);
    b = (zsword)var_read(18);
    if (mem[1] & 2) {               // a "time" game
        p = right;
        strcpy(p, "Time: ");
        p = fmtnum(p + 6, (a + 11) % 12 + 1);
        *p++ = ':';
        *p++ = (char)('0' + b / 10);
        *p++ = (char)('0' + b % 10);
        strcpy(p, a < 12 ? " am" : " pm");
    } else {
        p = right;
        strcpy(p, "Score: ");
        p = fmtnum(p + 7, a);
        strcpy(p, "  Moves: ");
        fmtnum(p + 9, b);
    }
    rlen = (byte)strlen(right);
    len = caplen;
    if (len + rlen + 4 > cols) len = cols > rlen + 4 ? cols - rlen - 4 : 0;
    save_cursor();
    ostr("\033[1;1H\033[7m ");
    for (i = 0; i < len; i++) ochar(name[i]);
    for (i = len + 1; i < cols - rlen - 1; i++) ochar(' ');
    ostr(right);
    ostr(" \033[0m\0338");
}

void tokenize(zword text, zword parse) {
    char seps[16];
    byte nsep, i, start, len, maxw, count = 0, c, j, n;
    byte zc[10];
    zword dict = h_dict, entries, nwords, w0, w1, lo, hi, mid, ea, e0, e1, found;
    nsep = rb(dict);
    entries = dict + 1 + nsep;
    if (nsep > sizeof seps) nsep = sizeof seps;
    for (i = 0; i < nsep; i++) seps[i] = (char)rb(dict + 1 + i);
    j = rb(entries);                // entry length
    nwords = rw(entries + 1);
    entries += 3;
    maxw = rb(parse);
    i = 1;
    for (;;) {
        c = rb(text + i);
        if (!c) break;
        if (c == ' ') {
            i++;
            continue;
        }
        start = i;
        if (memchr(seps, c, nsep)) {
            i++;
        } else {
            for (;;) {
                c = rb(text + i);
                if (!c || c == ' ' || memchr(seps, c, nsep)) break;
                i++;
            }
        }
        len = i - start;
        if (count >= maxw) continue;
        // Encode the first six z-characters of the word.
        n = 0;
        for (c = 0; c < len && n < 6; c++) {
            byte ch = rb(text + start + c);
            const char *q;
            if (ch >= 'a' && ch <= 'z') {
                zc[n++] = ch - 'a' + 6;
            } else if (ch != ' ' && ch != 13 && (q = (const char *)memchr(a2 + 2, ch, sizeof a2 - 3)) != 0) {
                zc[n++] = 5;
                zc[n++] = (byte)(q - a2 + 6);
            } else {
                zc[n++] = 5;
                zc[n++] = 6;
                zc[n++] = ch >> 5;
                zc[n++] = ch & 31;
            }
        }
        while (n < 6) zc[n++] = 5;
        w0 = ((zword)zc[0] << 10) | ((zword)zc[1] << 5) | zc[2];
        w1 = ((zword)zc[3] << 10) | ((zword)zc[4] << 5) | zc[5] | 0x8000;
        // Binary search: the dictionary is sorted.
        found = 0;
        lo = 0;
        hi = nwords;
        while (lo < hi) {
            mid = lo + ((hi - lo) >> 1);
            ea = entries + mid * j;
            e0 = rw(ea);
            e1 = rw(ea + 2);
            if (e0 == w0 && e1 == w1) {
                found = ea;
                break;
            }
            if (e0 < w0 || (e0 == w0 && e1 < w1)) lo = mid + 1;
            else hi = mid;
        }
        ea = parse + 2 + ((zword)count << 2);
        ww(ea, found);
        wb(ea + 2, len);
        wb(ea + 3, start);
        count++;
    }
    wb(parse + 1, count);
}

void sread(zword text, zword parse) {
    char buf[200];
    zword n, max, i;
    byte c;
    show_status();
    wflush();
    oflush();
    max = rb(text);
    if (max) max--;
    if (max > sizeof buf - 1) max = sizeof buf - 1;
    n = plat_readline(buf, max);
    if (n == 0xFFFF) finish(0);     // end of input, or ^C
    col = 0;
    lines = 0;
    if (row < rows) row++;
    for (i = 0; i < n; i++) {
        c = (byte)buf[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        wb(text + 1 + i, c);
    }
    wb(text + 1 + n, 0);
    tokenize(text, parse);
}

// ── Save, restore, restart, verify ─────────────────────────────────

const char *story_name;
char save_name[40];

byte ask_name(const char *what) {
    char buf[40];
    zword n;
    wflush();
    if (col) newline();
    zputs(what);
    zputs(" file name (");
    zputs(save_name);
    zputs("): ");
    wflush();
    oflush();
    n = plat_readline(buf, sizeof buf - 1);
    col = 0;
    lines = 0;
    if (n == 0xFFFF) return 0;
    if (n) {
        memcpy(save_name, buf, n);
        save_name[n] = 0;
    }
    return 1;
}

// A save file: "ZM3", the story's release, serial and checksum, then the
// dynamic memory, the stack and the call frames, and the PC.
void save_id(byte *id) {
    id[0] = 'Z';
    id[1] = 'M';
    id[2] = '3';
    memcpy(id + 3, mem + 2, 2);
    memcpy(id + 5, mem + 0x12, 6);
    memcpy(id + 11, mem + 0x1C, 2);
}

byte do_save(void) {
    byte id[13];
    byte ok;
    int f;
    if (!ask_name("Save")) return 0;
    f = plat_create(save_name);
    if (f < 0) return 0;
    save_id(id);
    get_pc();
    ok = plat_fwrite(f, id, 13) && plat_fwrite(f, mem, dyn_end) &&
         plat_fwrite(f, &sp, 2) && plat_fwrite(f, &bp, 2) &&
         plat_fwrite(f, &nloc, 1) && plat_fwrite(f, stk, sp << 1) &&
         plat_fwrite(f, &ncalls, 1) &&
         plat_fwrite(f, calls, ncalls * sizeof calls[0]) &&
         plat_fwrite(f, &pc_hi, 1) && plat_fwrite(f, &pc_lo, 2);
    plat_close(f);
    return ok;
}

byte do_restore(void) {
    byte id[13], want[13];
    zword f2;
    int f;
    if (!ask_name("Restore")) return 0;
    f = plat_openr(save_name);
    if (f < 0) return 0;
    save_id(want);
    if (!plat_fread(f, id, 13) || memcmp(id, want, 13)) {
        plat_close(f);
        zputs("[That is not a save file for this story.]");
        zputc(13);
        return 0;
    }
    f2 = rw(0x10);
    if (!(plat_fread(f, mem, dyn_end) && plat_fread(f, &sp, 2) &&
          plat_fread(f, &bp, 2) && plat_fread(f, &nloc, 1) &&
          sp <= STACK && plat_fread(f, stk, sp << 1) &&
          plat_fread(f, &ncalls, 1) && ncalls <= CALLS &&
          plat_fread(f, calls, ncalls * sizeof calls[0]) &&
          plat_fread(f, &pc_hi, 1) && plat_fread(f, &pc_lo, 2)))
        fatal("the save file is damaged");
    plat_close(f);
    mem[0x10] = (byte)(f2 >> 8);    // keep the transcript bits as they were
    mem[0x11] = (byte)((mem[0x11] & ~3) | (f2 & 3));
    setup_header();
    set_pc(pc_hi, pc_lo);
    return 1;
}

void load_dynamic(void) {
    byte pg;
    for (pg = 0; (zword)pg << 9 < dyn_end; pg++) plat_read_page(pg, mem + ((zword)pg << 9));
}

void restart(void) {
    zword f2 = rw(0x10);
    load_dynamic();
    mem[0x10] = (byte)(f2 >> 8);
    mem[0x11] = (byte)f2;
    setup_header();
    sp = bp = 0;
    nloc = 0;
    ncalls = 0;
    s3depth = 0;
    split_window(0);
    set_window(0);
    set_pc(0, rw(6));
}

// Sum the story file as it was, not as the game has changed it.
byte verify(void) {
    zword len = rw(0x1A), sum = 0, n, i, pg, pages;
    byte p[PAGE];
    if (!len) return 1;
    pages = (len >> 8) + ((len & 255) != 0);  // len is in words
    for (pg = 0; pg < pages; pg++) {
        plat_read_page((byte)pg, p);
        n = pg + 1 < pages || !(len & 255) ? PAGE : (len & 255) << 1;
        for (i = pg ? 0 : 0x40; i < n; i++) sum += p[i];
    }
    return sum == rw(0x1C);
}

// ── Arithmetic and random numbers ──────────────────────────────────

zword sdiv(zword a, zword b, byte want_mod) {
    zword ua = a, ub = b, q, r;
    byte na = (a & 0x8000) != 0, nb = (b & 0x8000) != 0;
    if (!b) fatal("division by zero");
    if (na) ua = (zword)(0 - a);
    if (nb) ub = (zword)(0 - b);
    q = ua / ub;
    r = ua % ub;
    if (want_mod) return na ? (zword)(0 - r) : r;
    return na != nb ? (zword)(0 - q) : q;
}

zword rnd_x = 1, rnd_seq, rnd_count;

zword zrandom(zword r) {
    zsword s = (zsword)r;
    if (s < 0) {
        if (s > -1000) {
            rnd_seq = (zword)(0 - r);
            rnd_count = 0;
        } else {
            rnd_seq = 0;
            rnd_x = (zword)(0 - r);
        }
        return 0;
    }
    if (s == 0) {
        rnd_seq = 0;
        rnd_x = plat_seed() | 1;
        return 0;
    }
    if (rnd_seq) {
        rnd_count = rnd_count % rnd_seq + 1;
        return (rnd_count - 1) % r + 1;
    }
    rnd_x ^= (zword)(rnd_x << 7);
    rnd_x ^= rnd_x >> 9;
    rnd_x ^= (zword)(rnd_x << 8);
    return rnd_x % r + 1;
}

// ── The interpreter ────────────────────────────────────────────────

void do_2op(byte op) {
    zword a = ops[0], b = ops[1], x;
    switch (op) {
    case 1:     // je
        branch(a == b || (nops > 2 && a == ops[2]) || (nops > 3 && a == ops[3]));
        break;
    case 2: branch((zsword)a < (zsword)b); break;            // jl
    case 3: branch((zsword)a > (zsword)b); break;            // jg
    case 4:     // dec_chk
        x = var_peek((byte)a) - 1;
        var_poke((byte)a, x);
        branch((zsword)x < (zsword)b);
        break;
    case 5:     // inc_chk
        x = var_peek((byte)a) + 1;
        var_poke((byte)a, x);
        branch((zsword)x > (zsword)b);
        break;
    case 6: branch(obj_parent(a) == b); break;               // jin
    case 7: branch((a & b) == b); break;                     // test
    case 8: store(a | b); break;                             // or
    case 9: store(a & b); break;                             // and
    case 10: branch(attr_test(a, b)); break;                 // test_attr
    case 11: attr_set(a, b, 1); break;                       // set_attr
    case 12: attr_set(a, b, 0); break;                       // clear_attr
    case 13: var_poke((byte)a, b); break;                    // store
    case 14: insert_obj(a, b); break;                        // insert_obj
    case 15: store(rw(a + (b << 1))); break;                 // loadw
    case 16: store(rb(a + b)); break;                        // loadb
    case 17: store(get_prop(a, b)); break;                   // get_prop
    case 18:    // get_prop_addr
        x = find_prop(a, b);
        store(x ? x + 1 : 0);
        break;
    case 19: store(next_prop(a, b)); break;                  // get_next_prop
    case 20: store(a + b); break;                            // add
    case 21: store(a - b); break;                            // sub
    case 22: store(a * b); break;                            // mul
    case 23: store(sdiv(a, b, 0)); break;                    // div
    case 24: store(sdiv(a, b, 1)); break;                    // mod
    default: fatal("illegal 2OP instruction");
    }
}

void do_1op(byte op) {
    zword a = ops[0], x;
    switch (op) {
    case 0: branch(a == 0); break;                           // jz
    case 1:     // get_sibling
        x = obj_sibling(a);
        store(x);
        branch(x != 0);
        break;
    case 2:     // get_child
        x = obj_child(a);
        store(x);
        branch(x != 0);
        break;
    case 3: store(obj_parent(a)); break;                     // get_parent
    case 4: store(prop_len(a)); break;                       // get_prop_len
    case 5: var_poke((byte)a, var_peek((byte)a) + 1); break; // inc
    case 6: var_poke((byte)a, var_peek((byte)a) - 1); break; // dec
    case 7: print_at(0, a); break;                           // print_addr
    case 9: remove_obj(a); break;                            // remove_obj
    case 10: print_obj(a); break;                            // print_obj
    case 11: zret(a); break;                                 // ret
    case 12: pc_add((zsword)a - 2); break;                   // jump
    case 13: print_at((byte)(a >> 15), a << 1); break;       // print_paddr
    case 14: store(var_peek((byte)a)); break;                // load
    case 15: store(~a); break;                               // not
    default: fatal("illegal 1OP instruction");
    }
}

void do_0op(byte op) {
    switch (op) {
    case 0: zret(1); break;                                  // rtrue
    case 1: zret(0); break;                                  // rfalse
    case 2:     // print
        zs_pc = 1;
        decode();
        break;
    case 3:     // print_ret
        zs_pc = 1;
        decode();
        zputc(13);
        zret(1);
        break;
    case 4: break;                                           // nop
    case 5: branch(do_save()); break;                        // save
    case 6: branch(do_restore()); break;                     // restore
    case 7: restart(); break;                                // restart
    case 8: zret(var_read(0)); break;                        // ret_popped
    case 9: var_read(0); break;                              // pop
    case 10: finish(0); break;                               // quit
    case 11: zputc(13); break;                               // new_line
    case 12: show_status(); break;                           // show_status
    case 13: branch(verify()); break;                        // verify
    case 15: branch(1); break;                               // piracy
    default: fatal("illegal 0OP instruction");
    }
}

void do_var(byte op) {
    zword a = ops[0], b = ops[1], x;
    zsword s;
    switch (op) {
    case 0: zcall(FETCH()); break;                           // call
    case 1: ww(a + (b << 1), ops[2]); break;                 // storew
    case 2: wb(a + b, (byte)ops[2]); break;                  // storeb
    case 3: put_prop(a, b, ops[2]); break;                   // put_prop
    case 4: sread(a, b); break;                              // sread
    case 5: zputc(a); break;                                 // print_char
    case 6: print_num(a); break;                             // print_num
    case 7: store(zrandom(a)); break;                        // random
    case 8: var_write(0, a); break;                          // push
    case 9:     // pull
        x = var_read(0);
        var_poke((byte)a, x);
        break;
    case 10: split_window(a); break;                         // split_window
    case 11: set_window(a); break;                           // set_window
    case 19:    // output_stream
        s = (zsword)a;
        if (s == 1) screen_on = 1;
        else if (s == -1) screen_on = 0;
        else if (s == 3) {
            if (s3depth == 16) fatal("output stream 3 nested too deeply");
            s3tab[s3depth] = b;
            s3len[s3depth++] = 0;
        } else if (s == -3 && s3depth) {
            s3depth--;
            ww(s3tab[s3depth], s3len[s3depth]);
        }
        break;
    case 20: break;                                          // input_stream
    case 21:    // sound_effect
        if (a == 1 || a == 2) {
            oflush();
            plat_beep(a == 1);
        }
        break;
    default: fatal("illegal VAR instruction");
    }
}

void run(void) {
    byte op, t, types, shift;
    for (;;) {
        op = FETCH();
        if (op < 0x80) {            // long form: 2OP
            ops[0] = (op & 0x40) ? var_read(FETCH()) : FETCH();
            ops[1] = (op & 0x20) ? var_read(FETCH()) : FETCH();
            nops = 2;
            do_2op(op & 0x1F);
        } else if (op < 0xC0) {     // short form
            t = (op >> 4) & 3;
            if (t == 3) {
                do_0op(op & 15);
            } else {
                if (t == 0) ops[0] = fetchw();
                else if (t == 1) ops[0] = FETCH();
                else ops[0] = var_read(FETCH());
                nops = 1;
                do_1op(op & 15);
            }
        } else {                    // variable form
            types = FETCH();
            nops = 0;
            for (shift = 8; shift;) {
                shift -= 2;
                t = (types >> shift) & 3;
                if (t == 3) break;
                if (t == 0) ops[nops] = fetchw();
                else if (t == 1) ops[nops] = FETCH();
                else ops[nops] = var_read(FETCH());
                nops++;
            }
            if (op < 0xE0) do_2op(op & 0x1F);
            else do_var(op & 0x1F);
        }
    }
}

// ── Errors and exit ────────────────────────────────────────────────

void fatal(const char *msg) {
    char b[8];
    const char *hex = "0123456789ABCDEF";
    get_pc();
    wflush();
    ostr("\r\n*** zm: ");
    ostr(msg);
    ostr(" (PC $");
    b[0] = hex[pc_hi & 15];
    b[1] = hex[(pc_lo >> 12) & 15];
    b[2] = hex[(pc_lo >> 8) & 15];
    b[3] = hex[(pc_lo >> 4) & 15];
    b[4] = hex[pc_lo & 15];
    b[5] = 0;
    ostr(b);
    ostr(") ***\r\n");
    finish(1);
}

void finish(byte code) {
    screen_done();
    plat_exit(code);
}

// ── Main ───────────────────────────────────────────────────────────

void usage(void) {
    ostr("Usage: zm [-w cols] [-h rows] [-m kbytes] [-d] story[.z3]\r\n");
    oflush();
    plat_exit(1);
}

// Open dir + story_name, or that with .z3 added (dir only for a bare name).
byte open_story(char *name, const char *dir) {
    if (*dir && strchr(story_name, '/')) return 0;
    strcpy(name, dir);
    strcat(name, story_name);
    if (plat_open_story(name)) return 1;
    strcat(name, ".z3");
    return plat_open_story(name);
}

zword number(const char *s) {
    zword n = 0;
    while (*s >= '0' && *s <= '9') n = n * 10 + (zword)(*s++ - '0');
    return n;
}

int main(int argc, char **argv) {
    char name[128];
    const char *base;
    char *dot;
    zword memk = 60, size, total, dynpages, resident = 0, i;
    byte *p;
    int ai;
    for (ai = 1; ai < argc && argv[ai][0] == '-'; ai++) {
        char o = argv[ai][1];
        if (o == 'd') {
            dumb = 1;
        } else if (ai + 1 < argc && (o == 'w' || o == 'h' || o == 'm')) {
            i = number(argv[++ai]);
            if (o == 'w' && i >= 20 && i <= 200) cols = (byte)i;
            else if (o == 'h' && i >= 8 && i <= 200) rows = (byte)i;
            else if (o == 'm' && i >= 8 && i <= 60) memk = i;
            else usage();
        } else {
            usage();
        }
    }
    if (ai != argc - 1 || strlen(argv[ai]) > 120) usage();
    story_name = argv[ai];
    if (!open_story(name, "") && !open_story(name, "/DD/GAMES/")) {
        ostr("zm: cannot open ");
        ostr(story_name);
        ostr("\r\n");
        oflush();
        plat_exit(1);
    }
    // The default save file: the story's name, with .sav for its extension.
    base = strrchr(name, '/');
    base = base ? base + 1 : name;
    strncpy(save_name, base, 30);
    save_name[30] = 0;
    dot = strrchr(save_name, '.');
    if (dot) *dot = 0;
    strcat(save_name, ".sav");
    plat_init();

    size = memk << 10;
    p = plat_alloc(&size);
    if (!p || size < 4096) {
        ostr("zm: not enough memory\r\n");
        oflush();
        plat_exit(1);
    }
    plat_read_page(0, p);
    if (p[0] != 3) {
        ostr("zm: this is a version ");
        onum(p[0]);
        ostr(" story; zm plays version 3\r\n");
        oflush();
        plat_exit(1);
    }
    dyn_end = ((zword)p[0x0E] << 8) | p[0x0F];
    npages = plat_story_pages();
    total = size >> 9;
    dynpages = (dyn_end >> 9) + ((dyn_end & 511) != 0);
    if (total >= npages && npages <= MAXSLOTS) {
        resident = npages;
    } else if (total >= dynpages + 4) {
        resident = dynpages;
        nslots = total - dynpages > MAXSLOTS ? MAXSLOTS : (byte)(total - dynpages);
    } else {
        ostr("zm: not enough memory for this story\r\n");
        oflush();
        plat_exit(1);
    }
    mem = p;
    rsize = resident << 9;
    slots = mem + rsize;
    for (i = 0; i < resident; i++) {
        if (i) plat_read_page((byte)i, mem + (i << 9));
        pmap[i] = mem + (i << 9);
    }
    h_dict = rw(0x08);
    h_objects = rw(0x0A);
    h_globals = rw(0x0C);
    h_abbrevs = rw(0x18);
    setup_header();
    rnd_x = plat_seed() | 1;

    screen_init();
    set_pc(0, rw(6));
    run();
    return 0;
}
