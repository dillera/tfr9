// vi.c -- a small vi for NitrOS-9 Level 2 on the TFR/911H.
//
//   vi [-w cols] [-h rows] [file]
//
// Normal mode
//   move     h j k l (or arrows)  w b e W B E  0 ^ $  G gg H M L
//            f F t T ; ,  %  { }  + - Enter  ^F ^B ^D ^U (or PgUp/PgDn)
//   search   /text ?text n N *    (plain text, not patterns)
//   insert   i a I A o O          (ESC ends)
//   change   x X D C s S r J ~ p P >> << u .
//            d c y > < with any motion: dw, c$, y3j, dG, d/text ...
//            dd cc yy, counts (3dd, 2d3w)
//   other    ZZ ZQ ^G ^L :
// Insert mode: Backspace, ^W (word), ^U (line), Enter, Tab, arrows.
// Ex: :w [file]  :w!  :q  :q!  :wq  :x  :e[!] [file]  :r file  :N  :$
//     :s/old/new/[g]  :%s/old/new/[g]  :help
//
// OS-9 text files end their lines with CR.  vi also reads files with LF or
// CR LF endings, and writes CR.
//
// The text is in a gap buffer, in memory got by growing the data area with
// F$Mem.  The screen is redrawn by comparing with a copy of what it shows,
// so only what changed is sent, and scrolling uses the terminal's scroll
// region.  The TFR/911H firmware sends arrow keys untranslated while bit 7
// of $FF42 is set (otherwise left and right arrive as BS and Tab).
//
// Build for OS-9:  cmoc --os9 --function-stack=0 --add-os9-stack-space=1024 -o vi vi.c
// For testing on Unix:  cc -o vi vi.c

#ifdef _CMOC_VERSION_
#include <cmoc.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#endif

typedef unsigned char byte;
typedef unsigned short word;

#define CR 13
#define ESC 27
#define K_UP 128            // the tether's codes for the arrow and page keys
#define K_DOWN 129
#define K_LEFT 130
#define K_RIGHT 131
#define K_PGUP 132
#define K_PGDN 133
#define MAXCOLS 160
#define MAXROWS 60

void finish(void);

// ── Platform: NitrOS-9 ─────────────────────────────────────────────

#ifdef _CMOC_VERSION_

byte term;                  // our path to /term, raw
byte sig;                   // the last signal (^C, ^E): ignored
byte old_ff42;

#define FF42 (*(byte *)0xFF42)

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

byte os9_create(const char *name) {
    char n[62];
    byte path = 255;
    os9name(n, name);
    asm {
        lda     #2              ; WRITE.
        ldb     #$0B            ; owner read+write, public read
        leax    :n
        os9     $83             ; I$Create
        bcs     @err
        sta     :path
@err
    }
    return path;
}

void os9_close(byte path) {
    asm {
        lda     :path
        os9     $8F             ; I$Close
    }
}

// Returns the count read, or 0 at end of file or on an error.
word os9_read(byte path, byte *buf, word n) {
    word got = 0;
    asm {
        lda     :path
        ldx     :buf
        pshs    y
        ldy     :n
        os9     $89             ; I$Read
        bcc     @ok
        ldy     #0
@ok
        sty     :got
        puls    y
    }
    return got;
}

byte os9_write(byte path, const void *buf, word n) {
    byte err = 0;
    if (!n) return 0;
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

// Set a file's size (to truncate what we rewrote).
void os9_setsize(byte path, word size) {
    asm {
        lda     :path
        ldb     #2              ; SS.Size
        ldx     #0
        pshs    u
        ldu     :size
        os9     $8E             ; I$SetStt
        puls    u
    }
}

word os9_mem(word size, word *top) {
    word got = 0, t = 0;
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
    byte opts[32];
    byte path;
    byte *flag = &sig;
    void (*handler)(void) = intercept;
    asm {
        ldx     :handler
        pshs    u
        ldu     :flag
        os9     $09             ; F$Icpt
        puls    u
    }
    term = os9_open("/term", 3);
    if (term == 255) term = 0;
    path = term;
    asm {
        lda     :path
        ldb     #0              ; SS.Opt
        leax    :opts
        os9     $8D             ; I$GetStt
    }
    opts[4] = 0;                // PD.EKO: no echo
    opts[7] = 0;                // PD.PAU: no page pause
    opts[12] = 0;               // PD.EOF
    asm {
        lda     :path
        ldb     #0
        leax    :opts
        os9     $8E             ; I$SetStt
    }
    old_ff42 = FF42;
    FF42 = old_ff42 | 0x80;     // arrow keys untranslated
}

void plat_done(void) {
    FF42 = old_ff42 & 0x7F;
    if (term) os9_close(term);
}

void plat_exit(byte code) {
    asm {
        ldb     :code
        os9     $06             ; F$Exit
    }
}

void plat_write(const char *p, word n) {
    os9_write(1, p, n);
}

byte plat_ready(void) {
    byte path = term, ok = 0;
    asm {
        lda     :path
        ldb     #1              ; SS.Ready
        os9     $8D             ; I$GetStt
        bcs     @no
        inc     :ok
@no
    }
    return ok;
}

word plat_getc(void) {
    byte c = 0;
    while (!os9_read(term, &c, 1)) {}
    return c;
}

void plat_size(byte *rows, byte *cols) { (void)rows; (void)cols; }

int plat_openr(const char *name) {
    byte p = os9_open(name, 1);
    return p == 255 ? -1 : p;
}

word plat_read(int f, byte *b, word n) { return os9_read((byte)f, b, n); }

// Open a file to rewrite it, keeping its attributes, or create it.
int plat_openw(const char *name) {
    byte p = os9_open(name, 2);
    if (p == 255) p = os9_create(name);
    return p == 255 ? -1 : p;
}

byte plat_writef(int f, const byte *b, word n) { return os9_write((byte)f, b, n) == 0; }

void plat_closew(int f, word size) {
    os9_setsize((byte)f, size);
    os9_close((byte)f);
}

void plat_close(int f) { os9_close((byte)f); }

byte *plat_alloc(word *size) {
    word cur, top, ntop, want;
    cur = os9_mem(0, &top);
    if (!cur) return 0;
    want = *size;
    if (want > 0xF000 - top) want = 0xF000 - top;
    for (; want >= 4096; want -= 512) {
        if (os9_mem(cur + want, &ntop)) {
            *size = ntop - top;
            return (byte *)top;
        }
    }
    return 0;
}

// ── Platform: Unix (for testing) ───────────────────────────────────

#else

struct termios saved_tio;
int pending = -1;

void plat_init(void) {
    struct termios t;
    signal(SIGINT, SIG_IGN);
    tcgetattr(0, &saved_tio);
    t = saved_tio;
    cfmakeraw(&t);
    tcsetattr(0, TCSANOW, &t);
}

void plat_done(void) { tcsetattr(0, TCSANOW, &saved_tio); }
void plat_exit(byte code) { exit(code); }

void plat_write(const char *p, word n) {
    if (write(1, p, n) < 0) exit(1);
}

byte wait_input(int ms) {
    fd_set f;
    struct timeval tv;
    FD_ZERO(&f);
    FD_SET(0, &f);
    tv.tv_sec = 0;
    tv.tv_usec = ms * 1000;
    return select(1, &f, 0, 0, &tv) > 0;
}

byte plat_ready(void) { return pending >= 0 || wait_input(0); }

int rawc(void) {
    unsigned char c;
    if (read(0, &c, 1) != 1) {
        finish();
    }
    return c;
}

// Arrow and page keys become the tether's codes, as on the TFR/911H.
word plat_getc(void) {
    int c, d;
    char seq[8];
    int n = 0;
    if (pending >= 0) {
        c = pending;
        pending = -1;
        return (word)c;
    }
    c = rawc();
    if (c == 127) return 8;
    if (c != ESC || !wait_input(50)) return (word)c;
    d = rawc();
    if (d != '[' && d != 'O') {
        pending = d;
        return ESC;
    }
    while (n < 7) {
        seq[n++] = (char)(c = rawc());
        if (isalpha(c) || c == '~') break;
    }
    seq[n] = 0;
    if (!strcmp(seq, "A")) return K_UP;
    if (!strcmp(seq, "B")) return K_DOWN;
    if (!strcmp(seq, "C")) return K_RIGHT;
    if (!strcmp(seq, "D")) return K_LEFT;
    if (!strcmp(seq, "5~")) return K_PGUP;
    if (!strcmp(seq, "6~")) return K_PGDN;
    return 0;
}

void plat_size(byte *rows, byte *cols) {
    struct winsize ws;
    if (!ioctl(1, TIOCGWINSZ, &ws) && ws.ws_row && ws.ws_col) {
        *rows = ws.ws_row > MAXROWS ? MAXROWS : (byte)ws.ws_row;
        *cols = ws.ws_col > MAXCOLS ? MAXCOLS : (byte)ws.ws_col;
    }
}

int plat_openr(const char *name) { return open(name, O_RDONLY); }
word plat_read(int f, byte *b, word n) {
    ssize_t got = read(f, b, n);
    return got > 0 ? (word)got : 0;
}

// Unix files get LF line endings.
int plat_openw(const char *name) { return open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644); }
byte plat_writef(int f, const byte *b, word n) {
    byte tmp[512];
    word i, k;
    while (n) {
        k = n > sizeof tmp ? sizeof tmp : n;
        for (i = 0; i < k; i++) tmp[i] = b[i] == CR ? '\n' : b[i];
        if (write(f, tmp, k) != k) return 0;
        b += k;
        n -= k;
    }
    return 1;
}
void plat_closew(int f, word size) { (void)size; close(f); }
void plat_close(int f) { close(f); }

byte *plat_alloc(word *size) { return malloc(*size); }

#endif

// ── Screen output ──────────────────────────────────────────────────

byte rows = 24, cols = 80, trows;
char obuf[256];
word olen;

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

char *fmtnum(char *p, word n) {
    char d[6];
    byte i = 0;
    do {
        d[i++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    while (i) *p++ = d[--i];
    *p = 0;
    return p;
}

void onum(word n) {
    char b[8];
    fmtnum(b, n);
    ostr(b);
}

void ogoto(byte r, byte c) {      // 1-based
    ochar(ESC);
    ochar('[');
    onum(r);
    ochar(';');
    onum(c);
    ochar('H');
}

// ── The text: a gap buffer ─────────────────────────────────────────

byte *buf;
word bufsize, gs, ge;       // the text is buf[0, gs) and buf[ge, bufsize)
#define LEN (bufsize - (ge - gs))

word cur, curline;          // the cursor, and its line (from 0)
word top, topline;          // the first line on the screen
word nlines;                // lines in the text: it always ends with CR
word want;                  // the column j and k aim for
byte keep_want;
byte modified;

byte ch(word p) {
    return p < gs ? buf[p] : buf[p + (ge - gs)];
}

// ch() without the call, for the inner loops (p must be a plain variable).
#define CH(p) ((p) < gs ? buf[p] : buf[(p) + (ge - gs)])

// What has changed on the screen since the last render.
byte dirty_all = 1;
word dirty_line = 0xFFFF;   // one line changed (0xFFFF: none)

void move_gap(word p) {
    if (p < gs) {
        memmove(buf + ge - (gs - p), buf + p, gs - p);
        ge -= gs - p;
        gs = p;
    } else if (p > gs) {
        memmove(buf + gs, buf + ge, p - gs);
        ge += p - gs;
        gs = p;
    }
}

word count_nl(word p, word n) {
    word c = 0, k;
    byte *q;
    while (n) {
        if (p < gs) {
            k = gs - p;
            if (k > n) k = n;
            q = buf + p;
        } else {
            k = n;
            q = buf + p + (ge - gs);
        }
        p += k;
        n -= k;
        while (k--)
            if (*q++ == CR) c++;
    }
    return c;
}

void copy_out(word p, byte *dst, word n) {
    while (n--) *dst++ = ch(p++);
}

word lstart(word p) {
    byte *q, *b;
    if (p > gs) {               // in the part after the gap
        q = buf + p + (ge - gs);
        b = buf + ge;
        while (q > b && q[-1] != CR) q--;
        if (q > b) return (word)(q - buf) - (ge - gs);
        p = gs;
    }
    q = buf + p;
    while (q > buf && q[-1] != CR) q--;
    return (word)(q - buf);
}

word lend(word p) {         // the line's CR
    byte *q, *e;
    if (p < gs) {
        q = buf + p;
        e = buf + gs;
        while (q < e && *q != CR) q++;
        if (q < e) return (word)(q - buf);
        p = gs;
    }
    q = buf + p + (ge - gs);
    while (*q != CR) q++;
    return (word)(q - buf) - (ge - gs);
}

word first_nb(word p) {
    byte c;
    for (;;) {
        c = CH(p);
        if (c != ' ' && c != 9) return p;
        p++;
    }
}

// The start of line n (from 0), clamped to the last line.
word line_pos(word n) {
    word p, k;
    if (n >= nlines) n = nlines - 1;
    if (n >= curline) {
        p = lstart(cur);
        k = n - curline;
    } else if (n < curline - n) {
        p = 0;
        k = n;
    } else {
        p = lstart(cur);
        for (k = curline - n; k; k--) p = lstart(p - 1);
        return p;
    }
    for (; k; k--) p = lend(p) + 1;
    return p;
}

void set_cur(word p) {
    if (p > cur) curline += count_nl(cur, p - cur);
    else if (p < cur) curline -= count_nl(p, cur - p);
    cur = p;
}

word vcol(word p) {         // p's screen column, before scrolling sideways
    word q = lstart(p), c = 0;
    byte x;
    for (; q < p; q++) {
        x = CH(q);
        if (x == 9) c += 8 - (c & 7);
        else if (x < 32) c += 2;
        else c++;
    }
    return c;
}

word col_pos(word p, word col, byte at_eol) {
    word c = 0, w;
    byte x;
    for (;;) {
        x = ch(p);
        if (x == CR) break;
        if (x == 9) w = 8 - (c & 7);
        else if (x < 32) w = 2;
        else w = 1;
        if (c + w > col) return p;
        c += w;
        p++;
    }
    if (!at_eol && p && ch(p - 1) != CR) p--;
    return p;
}

// ── Messages ───────────────────────────────────────────────────────

char msg[MAXCOLS];

void say(const char *a, const char *b) {
    word n = 0;
    while (*a && n < sizeof msg - 1) msg[n++] = *a++;
    while (b && *b && n < sizeof msg - 1) msg[n++] = *b++;
    msg[n] = 0;
}

void say_file(const char *name, word lines, word bytes, const char *tail) {
    char b[60], *p = b;
    *p++ = ' ';
    p = fmtnum(p, lines);
    *p++ = 'L';
    *p++ = ',';
    *p++ = ' ';
    p = fmtnum(p, bytes);
    *p++ = 'B';
    strcpy(p, tail);
    say("\"", name);
    strcat(msg, "\"");
    strcat(msg, b);
}

// ── Undo ───────────────────────────────────────────────────────────

// Records: seq(2) pos(2) len(2) type(1) [deleted text] size(2).  Each
// command has its own seq, and u undoes all the records of the last one.
#define UREC 9
byte *ubuf;
word ucap, un, useq;
byte undoing, no_undo;

word get16(byte *p) { return ((word)p[0] << 8) | p[1]; }
void put16(byte *p, word w) { p[0] = (byte)(w >> 8); p[1] = (byte)w; }

void urec(byte type, word pos, word len) {
    word size;
    byte *r;
    if (undoing || no_undo) return;
    if (type == 'i' && un) {        // typing: grow the last insert
        r = ubuf + un - get16(ubuf + un - 2);
        if (r[6] == 'i' && get16(r) == useq && get16(r + 2) + get16(r + 4) == pos) {
            put16(r + 4, get16(r + 4) + len);
            return;
        }
    }
    size = UREC + (type == 'd' ? len : 0);
    if (size > ucap) {              // too big to undo: forget everything
        un = 0;
        return;
    }
    while (un + size > ucap) {      // drop the oldest
        word old = UREC + (ubuf[6] == 'd' ? get16(ubuf + 4) : 0);
        memmove(ubuf, ubuf + old, un - old);
        un -= old;
    }
    r = ubuf + un;
    put16(r, useq);
    put16(r + 2, pos);
    put16(r + 4, len);
    r[6] = type;
    if (type == 'd') copy_out(pos, r + 7, size - UREC);
    put16(r + size - 2, size);
    un += size;
}

// ── Editing ────────────────────────────────────────────────────────

// Note an edit at pos, which adds or removes nl lines, for render().
void touch(word pos, word nl) {
    if (dirty_all) return;
    if (!nl && pos >= lstart(cur) && pos <= lend(cur)) {
        if (dirty_line == 0xFFFF || dirty_line == curline) {
            dirty_line = curline;
            return;
        }
    }
    dirty_all = 1;
}

byte ins(word pos, const byte *s, word n) {
    word nl = 0, i;
    if (n > ge - gs) {
        say("Out of memory: the text is too big", 0);
        return 0;
    }
    for (i = 0; i < n; i++)
        if (s[i] == CR) nl++;
    touch(pos, nl);
    urec('i', pos, n);
    if (pos < cur) {
        cur += n;
        curline += nl;
    }
    if (pos < top) {
        top += n;
        topline += nl;
    }
    move_gap(pos);
    memcpy(buf + gs, s, n);
    gs += n;
    nlines += nl;
    modified = 1;
    return 1;
}

void del(word pos, word n) {
    word e = pos + n;
    if (!n) return;
    touch(pos, count_nl(pos, n));
    urec('d', pos, n);
    if (pos < cur) {
        curline -= count_nl(pos, (cur < e ? cur : e) - pos);
        cur = cur >= e ? cur - n : pos;
    }
    if (pos < top) {
        topline -= count_nl(pos, (top < e ? top : e) - pos);
        top = top >= e ? top - n : pos;
    }
    nlines -= count_nl(pos, n);
    move_gap(pos);
    ge += n;
    top = lstart(top);
    modified = 1;
}

void ins_char(word pos, byte c) {
    ins(pos, &c, 1);
}

void ensure_final_cr(void) {
    if (!LEN || ch(LEN - 1) != CR) ins_char(LEN, CR);
}

void undo(void) {
    byte *r;
    word seq, pos = cur;
    if (!un) {
        say("Already at oldest change", 0);
        return;
    }
    seq = get16(ubuf + un - get16(ubuf + un - 2));
    undoing = 1;
    while (un) {
        r = ubuf + un - get16(ubuf + un - 2);
        if (get16(r) != seq) break;
        pos = get16(r + 2);
        if (r[6] == 'i') del(pos, get16(r + 4));
        else ins(pos, r + 7, get16(r + 4));
        un = (word)(r - ubuf);
    }
    undoing = 0;
    ensure_final_cr();
    if (pos >= LEN) pos = LEN - 1;
    set_cur(pos);
}

// The unnamed register.
byte *ybuf;
word ycap, yn;
byte ylines;

void yank(word p, word n, byte lines) {
    if (n > ycap) {
        say("Too much to yank", 0);
        return;
    }
    copy_out(p, ybuf, n);
    yn = n;
    ylines = lines;
}

// ── Display ────────────────────────────────────────────────────────

byte *shadow;               // what the screen shows, rows x cols
word leftcol;
byte mode_insert;
word force_top = 0xFFFF;    // a top line asked for by ^F and friends
byte raw_seen;              // arrow keys have come untranslated

void row_out(byte r, byte *line) {
    byte *sh = shadow + (word)r * cols;
    byte i0, last, i, blank = 1;
    for (i0 = 0; i0 < cols && line[i0] == sh[i0]; i0++) {}
    if (i0 == cols) return;
    for (last = cols; last > i0 && line[last - 1] == ' '; last--) {}
    for (i = last; i < cols; i++)
        if (sh[i] != ' ') blank = 0;
    ogoto(r + 1, i0 + 1);
    for (i = i0; i < last; i++) ochar((char)line[i]);
    if (!blank) ostr("\033[K");
    memcpy(sh, line, cols);
}

// Draw the line at p (have = 0: past the end of the text) on row r.
// Returns the start of the next line.
word draw_row(byte r, word p, byte have) {
    byte line[MAXCOLS];
    word c = 0, right = leftcol + cols;
    byte x, g[8], gn, i;
    memset(line, ' ', cols);
    if (!have) {
        line[0] = '~';
    } else {
        for (;;) {
            if (c >= right) {
                p = lend(p) + 1;
                break;
            }
            x = CH(p);
            p++;
            if (x == CR) break;
            if (x == 9) {
                gn = (byte)(8 - (c & 7));
                memset(g, ' ', gn);
            } else if (x < 32) {
                g[0] = '^';
                g[1] = x + 64;
                gn = 2;
            } else if (x > 126) {
                g[0] = '?';
                gn = 1;
            } else {
                g[0] = x;
                gn = 1;
            }
            for (i = 0; i < gn; i++, c++)
                if (c >= leftcol && c < right) line[c - leftcol] = g[i];
        }
    }
    row_out(r, line);
    return p;
}

void scroll_rows(int d) {   // d > 0: the text moves up
    word n;
    if (d > 0) {
        ogoto(trows, 1);
        for (n = 0; n < (word)d; n++) ochar('\n');
        n = (word)d * cols;
        memmove(shadow, shadow + n, (word)trows * cols - n);
        memset(shadow + (word)trows * cols - n, ' ', n);
    } else {
        d = -d;
        ogoto(1, 1);
        for (n = 0; n < (word)d; n++) ostr("\033M");
        n = (word)d * cols;
        memmove(shadow + n, shadow, (word)trows * cols - n);
        memset(shadow, ' ', n);
    }
}

void clear_screen(void) {
    ostr("\033[0m\033[2J");
    memset(shadow, ' ', (word)rows * cols);
    dirty_all = 1;
}

void render(void) {
    word p, ccol, newtop, i, half = trows / 2;
    static word drawn_left = 0xFFFF;
    int d;
    byte r, line[MAXCOLS];
    char b[24], *q;
    // Scroll to keep the cursor on the screen.
    newtop = force_top != 0xFFFF ? force_top : topline;
    force_top = 0xFFFF;
    if (curline < newtop) newtop = newtop - curline > half ? (curline > half ? curline - half : 0) : curline;
    else if (curline >= newtop + trows) newtop = curline - (newtop + trows - 1) > half ? curline - half : curline - trows + 1;
    if (newtop != topline) {
        d = (int)newtop - (int)topline;
        if (d > 0 && d < trows) {
            for (i = 0; i < (word)d; i++) top = lend(top) + 1;
            scroll_rows(d);
        } else if (d < 0 && -d < trows) {
            for (i = 0; i < (word)-d; i++) top = lstart(top - 1);
            scroll_rows(d);
        } else {
            top = line_pos(newtop);
        }
        topline = newtop;
        dirty_all = 1;
    }
    ccol = vcol(cur);
    if (!mode_insert && ch(cur) == 9) ccol += 7 - (ccol & 7);
    if (ccol < leftcol) leftcol = ccol > cols / 2 ? ccol - cols / 2 : 0;
    else if (ccol >= leftcol + cols) leftcol = ccol - cols / 2;
    if (leftcol != drawn_left) dirty_all = 1;
    drawn_left = leftcol;
    if (dirty_all) {                    // every row
        p = top;
        for (r = 0; r < trows; r++) {
            if (p < LEN) p = draw_row(r, p, 1);
            else draw_row(r, 0, 0);
        }
    } else if (dirty_line != 0xFFFF && dirty_line >= topline && dirty_line < topline + trows) {
        draw_row((byte)(dirty_line - topline), line_pos(dirty_line), 1);
    }
    dirty_all = 0;
    dirty_line = 0xFFFF;
    // The bottom line: a message or the mode, and the cursor's place.
    memset(line, ' ', cols);
    q = msg[0] ? msg : mode_insert ? "-- INSERT --" : "";
    for (i = 0; q[i] && i < cols; i++) line[i] = (byte)q[i];
    q = fmtnum(b, curline + 1);
    *q++ = ',';
    fmtnum(q, (word)(ccol + 1));
    i = cols > 18 ? cols - 18 : 0;
    if (i < (word)strlen(msg) + 1) i = (word)strlen(msg) + 1;
    for (q = b; *q && i < cols; i++) line[i] = (byte)*q++;
    row_out(trows, line);
    ogoto((byte)(curline - topline + 1), (byte)(ccol - leftcol + 1));
    oflush();
}

// ── Keys ───────────────────────────────────────────────────────────

byte replay[512];           // keys to replay (for .)
word rp_n, rp_i;
byte rec[512];              // the keys of the command being done
word rec_n;
byte rec_over;
byte dot[512];
word dot_n;

word getkey(byte show) {
    word c;
    if (rp_i < rp_n) {
        c = replay[rp_i++];
    } else {
        if (show && !plat_ready()) render();
        oflush();
        c = plat_getc();
    }
    if (c >= K_UP && c <= K_PGDN) raw_seen = 1;
    if (rec_n < sizeof rec) rec[rec_n++] = (byte)c;
    else rec_over = 1;
    return c;
}

// Read a line on the bottom row.  Returns its length, or -1 if cancelled.
int read_line(char prompt, char *b, int max) {
    int n = 0;
    word c;
    ogoto(rows, 1);
    ochar(prompt);
    ostr("\033[K");
    memset(shadow + (word)trows * cols, 0, cols);  // redraw it afterwards
    for (;;) {
        c = getkey(0);
        if (c == CR) break;
        if (c == ESC) return -1;
        if (c == 8 || c == 127) {
            if (!n) return -1;
            n--;
            ostr("\b \b");
        } else if (c == 21) {
            while (n) {
                n--;
                ostr("\b \b");
            }
        } else if (c >= 32 && c < 127 && n < max) {
            b[n++] = (char)c;
            ochar((char)c);
        }
    }
    b[n] = 0;
    return n;
}

// ── Files ──────────────────────────────────────────────────────────

char fname[128];

// Insert a file's text at pos.  Returns 0 if it can't be opened, 1 if read,
// 2 if it didn't fit.  *lines and *bytes say how much was inserted.
byte load(const char *name, word pos, word *lines, word *bytes) {
    word n, i, k, want, nl, start = pos;
    byte *q, c, last_cr = 0, ok = 1;
    int f = plat_openr(name);
    *lines = *bytes = 0;
    if (f < 0) return 0;
    move_gap(pos);                      // read straight into the gap
    dirty_all = 1;
    for (;;) {
        want = ge - gs;
        if (want < 2) {
            say("Out of memory: the text is too big", 0);
            ok = 0;
            break;
        }
        want--;                         // leave room for a final CR
        if (want > 4096) want = 4096;
        q = buf + gs;
        n = plat_read(f, q, want);
        if (!n) break;
        for (i = k = nl = 0; i < n; i++) {  // CR LF and LF become CR
            c = q[i];
            if (c == '\n') {
                if (last_cr) {
                    last_cr = 0;
                    continue;
                }
                c = CR;
            } else {
                last_cr = c == CR;
            }
            if (c == CR) nl++;
            q[k++] = c;
        }
        urec('i', gs, k);               // as ins() does
        if (gs < cur) {
            cur += k;
            curline += nl;
        }
        if (gs < top) {
            top += k;
            topline += nl;
        }
        gs += k;
        nlines += nl;
        modified = 1;
    }
    plat_close(f);
    if (gs > start && buf[gs - 1] != CR) ins_char(gs, CR);
    *bytes = gs - start;
    *lines = count_nl(start, gs - start);
    return ok ? 1 : 2;
}

byte save(const char *name, word *lines, word *bytes) {
    word n = LEN;
    byte ok;
    int f = plat_openw(name);
    if (f < 0) return 0;
    if (n == 1) n = 0;              // just one empty line: an empty file
    ok = !n || (plat_writef(f, buf, gs) && plat_writef(f, buf + ge, bufsize - ge));
    plat_closew(f, n);
    *lines = nlines;
    *bytes = n;
    return ok;
}

void clear_text(void) {
    gs = 0;
    ge = bufsize;
    cur = curline = top = topline = nlines = 0;
    un = 0;
    yn = 0;
}

void edit_file(const char *name) {
    word lines, bytes;
    byte r;
    clear_text();
    no_undo = 1;
    r = name[0] ? load(name, 0, &lines, &bytes) : 0;
    no_undo = 0;
    ensure_final_cr();
    modified = 0;
    if (!name[0]) msg[0] = 0;
    else if (r == 0) {
        say("\"", name);
        strcat(msg, "\" [New File]");
    }
    else if (r == 2) say_file(name, lines, bytes, " [too big: cut short]");
    else say_file(name, lines, bytes, "");
    set_cur(0);
}

byte write_file(const char *name) {
    word lines, bytes;
    if (!name[0]) {
        say("No file name", 0);
        return 0;
    }
    if (!save(name, &lines, &bytes)) {
        say("Can't write ", name);
        return 0;
    }
    if (!fname[0]) strcpy(fname, name);
    if (!strcmp(name, fname)) modified = 0;
    say_file(name, lines, bytes, " written");
    return 1;
}

// ── Motions ────────────────────────────────────────────────────────

#define M_LINE 1            // linewise
#define M_INCL 2            // includes the character it lands on
byte mflags;
byte f_cmd, f_char;         // the last f, F, t or T
char pat[64];
byte patlen, pat_back;

byte cls(byte c, byte big) {
    if (c == ' ' || c == 9 || c == CR) return 0;
    if (big || isalnum(c) || c == '_') return 1;
    return 2;
}

word word_fwd(word p, byte big) {
    word last = LEN - 1;
    byte k = cls(ch(p), big), c;
    if (k)
        while (p < last && cls(ch(p), big) == k) p++;
    while (p < last) {          // skip blanks, but stop at an empty line
        c = ch(p);
        if (c == CR) {
            p++;
            if (ch(p) == CR) break;
            continue;
        }
        if (c != ' ' && c != 9) break;
        p++;
    }
    return p;
}

word word_back(word p, byte big) {
    byte k, c;
    if (!p) return 0;
    p--;
    while (p) {
        c = ch(p);
        if (c == CR) {
            if (ch(p - 1) == CR) break;
            p--;
            continue;
        }
        if (c != ' ' && c != 9) break;
        p--;
    }
    k = cls(ch(p), big);
    if (k)
        while (p && cls(ch(p - 1), big) == k) p--;
    return p;
}

word word_end(word p, byte big) {
    word last = LEN - 1;
    byte k;
    if (p >= last) return p;
    p++;
    while (p < last && !cls(ch(p), big)) p++;
    k = cls(ch(p), big);
    if (k)
        while (p < last && cls(ch(p + 1), big) == k) p++;
    return p;
}

byte match_at(word p) {
    byte i;
    if (p + patlen > LEN) return 0;
    for (i = 0; i < patlen; i++)
        if (ch(p + i) != (byte)pat[i]) return 0;
    return 1;
}

// Find pat from p (not at p), wrapping around.
byte search(byte back, word *t) {
    word p = cur, n = LEN, i;
    if (!patlen) {
        say("No previous search", 0);
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (back) p = p ? p - 1 : n - 1;
        else p = p + 1 < n ? p + 1 : 0;
        if (ch(p) == (byte)pat[0] && match_at(p)) {
            if (back ? p > cur : p < cur) say(back ? "search hit TOP, continuing at BOTTOM" : "search hit BOTTOM, continuing at TOP", 0);
            *t = p;
            return 1;
        }
    }
    say("Pattern not found: ", pat);
    return 0;
}

byte find_char(byte cmd, byte c, word n, word *t) {
    word p = cur, ls = lstart(cur), e = lend(cur);
    byte back = cmd == 'F' || cmd == 'T';
    while (n--) {
        do {
            if (back) {
                if (p == ls) return 0;
                p--;
            } else {
                if (p + 1 >= e) return 0;
                p++;
            }
        } while (ch(p) != c);
    }
    if (cmd == 't') p--;
    if (cmd == 'T') p++;
    *t = p;
    return 1;
}

byte match_bracket(word *t) {
    const char *pairs = "()[]{}";
    word p = cur, e = lend(cur), depth = 0;
    byte c, open, close, fwd, i;
    for (;; p++) {              // the first bracket from the cursor on
        if (p >= e) return 0;
        c = ch(p);
        if (strchr(pairs, c)) break;
    }
    for (i = 0; pairs[i] != (char)c; i++) {}
    fwd = !(i & 1);
    open = (byte)pairs[i & ~1];
    close = (byte)pairs[i | 1];
    for (;;) {
        c = ch(p);
        if (c == open) depth += fwd ? 1 : 0xFFFF;
        else if (c == close) depth += fwd ? 0xFFFF : 1;
        if (!depth) break;
        if (fwd) {
            if (++p >= LEN) return 0;
        } else {
            if (!p) return 0;
            p--;
        }
    }
    *t = p;
    return 1;
}

byte is_empty_line(word p) { return ch(p) == CR && (!p || ch(p - 1) == CR); }

word paragraph(byte fwd, word n) {
    word p = lstart(cur);
    while (n--) {
        if (fwd) {
            while (p < LEN - 1 && is_empty_line(p)) p = lend(p) + 1;
            while (p < LEN - 1 && !is_empty_line(p)) p = lend(p) + 1;
            if (p >= LEN - 1) p = LEN - 1;
        } else {
            while (p && is_empty_line(p)) p = lstart(p - 1);
            while (p && !is_empty_line(p)) p = lstart(p - 1);
        }
    }
    return p;
}

word count_lines_on_screen(void) {
    return nlines - topline < trows ? nlines - topline : trows;
}

// Where motion c goes from the cursor.  Returns 0 if it can't.
byte motion(word c, word n, byte hc, byte op, word *t) {
    word p = cur, ls, e, l, prev = cur;
    byte k;
    mflags = 0;
    switch (c) {
    case 'h': case K_LEFT: case 8:
        ls = lstart(cur);
        if (cur == ls) return 0;
        p = cur - ls > n ? cur - n : ls;
        break;
    case 'l': case K_RIGHT: case ' ': case 9:
        e = lend(cur);
        l = op || e == lstart(cur) ? e : e - 1;
        if (cur >= l) return 0;
        p = l - cur > n ? cur + n : l;
        break;
    case 'j': case K_DOWN: case 10: case 14:
    case 'k': case K_UP: case 12: case 16:
        if (c == 12 && raw_seen) return 0;
        if (c == 'j' || c == K_DOWN || c == 10 || c == 14) {
            if (curline + 1 >= nlines) return 0;
            l = nlines - 1 - curline > n ? curline + n : nlines - 1;
        } else {
            if (!curline) return 0;
            l = curline > n ? curline - n : 0;
        }
        p = col_pos(line_pos(l), want, op != 0);
        mflags = M_LINE;
        keep_want = 1;
        break;
    case '+': case CR: case '-':
        if (c == '-') {
            if (!curline) return 0;
            l = curline > n ? curline - n : 0;
        } else {
            if (curline + 1 >= nlines) return 0;
            l = nlines - 1 - curline > n ? curline + n : nlines - 1;
        }
        p = first_nb(line_pos(l));
        mflags = M_LINE;
        break;
    case '0': p = lstart(cur); break;
    case '^': p = first_nb(lstart(cur)); break;
    case '$':
        if (n > 1) p = line_pos(curline + n - 1 < nlines ? curline + n - 1 : nlines - 1);
        e = lend(p);
        ls = lstart(e);
        p = e > ls ? e - 1 : e;
        if (e > ls) mflags = M_INCL;
        want = 0xFFFF;
        keep_want = 1;
        break;
    case 'w': case 'W':
        while (n--) {
            prev = p;
            p = word_fwd(p, c == 'W');
        }
        if (op && p > lend(prev)) p = lend(prev);    // dw at a line's end
        if (p == cur && !op) return 0;
        break;
    case 'b': case 'B':
        while (n--) p = word_back(p, c == 'B');
        if (p == cur) return 0;
        break;
    case 'e': case 'E':
        while (n--) p = word_end(p, c == 'E');
        mflags = M_INCL;
        break;
    case 'G':
        p = first_nb(line_pos(hc ? n - 1 : nlines - 1));
        mflags = M_LINE;
        break;
    case 'g':
        if (getkey(0) != 'g') return 0;
        p = first_nb(line_pos(hc ? n - 1 : 0));
        mflags = M_LINE;
        break;
    case 'H': case 'L': case 'M':
        l = count_lines_on_screen();
        if (c == 'H') l = hc && n <= l ? n - 1 : 0;
        else if (c == 'L') l = hc && n <= l ? l - n : l - 1;
        else l = (l - 1) / 2;
        p = first_nb(line_pos(topline + l));
        mflags = M_LINE;
        break;
    case 'f': case 'F': case 't': case 'T':
        k = (byte)getkey(0);
        if (k == ESC) return 0;
        f_cmd = (byte)c;
        f_char = k;
        if (!find_char(f_cmd, f_char, n, &p)) return 0;
        if (c == 'f' || c == 't') mflags = M_INCL;
        break;
    case ';': case ',':
        if (!f_cmd) return 0;
        k = f_cmd;
        if (c == ',') k = k == 'f' ? 'F' : k == 'F' ? 'f' : k == 't' ? 'T' : 't';
        if (!find_char(k, f_char, n, &p)) return 0;
        if (k == 'f' || k == 't') mflags = M_INCL;
        break;
    case '%':
        if (!match_bracket(&p)) return 0;
        mflags = M_INCL;
        break;
    case '{': case '}':
        p = paragraph(c == '}', n);
        break;
    case '/': case '?':
        if (read_line((char)c, pat, sizeof pat - 1) < 0) return 0;
        if (pat[0]) patlen = (byte)strlen(pat);
        pat_back = c == '?';
        if (!search(pat_back, &p)) return 0;
        break;
    case 'n': case 'N':
        if (!search(pat_back ^ (c == 'N'), &p)) return 0;
        break;
    case '*':
        p = cur;
        if (!cls(ch(p), 0)) return 0;
        while (p && cls(ch(p - 1), 0) == cls(ch(cur), 0)) p--;
        for (patlen = 0; patlen < sizeof pat - 1 && cls(ch(p), 0) == cls(ch(cur), 0); p++) pat[patlen++] = (char)ch(p);
        pat[patlen] = 0;
        pat_back = 0;
        if (!search(0, &p)) return 0;
        break;
    default:
        return 0;
    }
    *t = p;
    return 1;
}

// ── Commands ───────────────────────────────────────────────────────

byte changed;               // the command changed the text (so . repeats it)

void insert_mode(word count) {
    word c, start = cur, i, n, k;
    byte moved = 0, tb[64];
    mode_insert = 1;
    for (;;) {
        c = getkey(1);
        msg[0] = 0;
        if (c == ESC) break;
        if (c == K_LEFT || c == K_RIGHT || c == K_UP || c == K_DOWN ||
            c == 10 || (c == 12 && !raw_seen)) {
            if (c == K_LEFT) {
                if (cur > lstart(cur)) set_cur(cur - 1);
            } else if (c == K_RIGHT) {
                if (ch(cur) != CR) set_cur(cur + 1);
            } else if ((c == K_UP || c == 12) && curline) {
                set_cur(col_pos(line_pos(curline - 1), vcol(cur), 1));
            } else if ((c == K_DOWN || c == 10) && curline + 1 < nlines) {
                set_cur(col_pos(line_pos(curline + 1), vcol(cur), 1));
            }
            moved = 1;
            start = cur;
        } else if (c == 8 || c == 127) {
            if (cur) del(cur - 1, 1);
        } else if (c == 23) {           // ^W: the word before the cursor
            n = cur;
            while (n > lstart(cur) && (ch(n - 1) == ' ' || ch(n - 1) == 9)) n--;
            if (n > lstart(cur) && cls(ch(n - 1), 0)) {
                k = cls(ch(n - 1), 0);
                while (n > lstart(cur) && cls(ch(n - 1), 0) == k) n--;
            }
            if (n == cur && cur) n = cur - 1;
            del(n, cur - n);
        } else if (c == 21) {           // ^U: back to the start of the line
            del(lstart(cur), cur - lstart(cur));
        } else if (c == CR || c == 9 || (c >= 32 && c < 127)) {
            tb[0] = (byte)c;
            if (ins(cur, tb, 1)) set_cur(cur + 1);
        }
    }
    mode_insert = 0;
    // 3ifoo<ESC> inserts foo three times.
    if (count > 1 && !moved && cur > start && cur - start <= sizeof tb) {
        n = cur - start;
        copy_out(start, tb, n);
        for (i = 1; i < count; i++)
            if (ins(cur, tb, n)) set_cur(cur + n);
    }
    if (cur > lstart(cur)) set_cur(cur - 1);
    changed = 1;
}

void indent_lines(word a, word b, byte out) {   // lines from a up to b
    word p = a, n;
    byte tab = 9;
    while (p < b) {
        if (out) {
            n = 0;
            if (ch(p) == 9) n = 1;
            else
                while (n < 8 && ch(p + n) == ' ') n++;
            del(p, n);
            b -= n;
        } else if (ch(p) != CR) {
            ins(p, &tab, 1);
            b++;
        }
        p = lend(p) + 1;
    }
}

// An operator and its motion (or preset, for D C s S Y).
void operator(byte op, word count, byte hc, byte preset) {
    word c, t, a, b, n2 = 0;
    byte lines;
    c = preset ? preset : getkey(0);
    while (!preset && ((c >= '1' && c <= '9') || (n2 && c == '0'))) {
        n2 = n2 * 10 + (c - '0');
        c = getkey(0);
    }
    if (n2) {
        count *= n2;
        hc = 1;
    }
    if (c == op) {                      // dd, cc, yy, >>, <<
        t = line_pos(curline + count - 1);
        mflags = M_LINE;
    } else {
        if (op == 'c' && (c == 'w' || c == 'W') && cls(ch(cur), 0)) c = c == 'w' ? 'e' : 'E';
        if (!motion(c, count, hc, op, &t)) return;
    }
    lines = mflags & M_LINE;
    a = cur < t ? cur : t;
    b = cur < t ? t : cur;
    if (lines) {
        a = lstart(a);
        b = lend(b) + 1;
    } else if (mflags & M_INCL) {
        if (ch(b) != CR) b++;
    }
    switch (op) {
    case 'y':
        yank(a, b - a, lines);
        if (t < cur) set_cur(t);
        return;
    case 'd':
        yank(a, b - a, lines);
        del(a, b - a);
        ensure_final_cr();
        if (a >= LEN) a = lstart(LEN - 1);
        set_cur(lines ? first_nb(lstart(a)) : a);
        break;
    case 'c':
        if (lines) b--;                 // keep a line to type on
        yank(a, b - a, lines);
        del(a, b - a);
        set_cur(a);
        insert_mode(1);
        break;
    case '>': case '<':
        indent_lines(lstart(a), lines ? b : lend(b) + 1, op == '<');
        set_cur(first_nb(lstart(cur)));
        break;
    }
    changed = 1;
}

void put(byte after, word count) {
    word p;
    if (!yn) {
        say("Nothing to put", 0);
        return;
    }
    if (ylines) {
        p = after ? lend(cur) + 1 : lstart(cur);
        while (count--) ins(p, ybuf, yn);
        set_cur(first_nb(p));
    } else {
        p = cur;
        if (after && ch(cur) != CR) p++;
        set_cur(p);
        while (count--)
            if (ins(cur, ybuf, yn)) set_cur(cur + yn);
        if (cur > p) set_cur(cur - 1);
    }
    changed = 1;
}

void join(word count) {
    word e, p;
    byte sp = ' ';
    if (count < 2) count = 2;
    while (--count) {
        e = lend(cur);
        if (e + 1 >= LEN) return;
        p = first_nb(e + 1);
        del(e, p - e);                  // the CR and the next line's indent
        if (e > lstart(e) && ch(e - 1) != ' ' && ch(e) != ')' && ch(e) != CR) ins(e, &sp, 1);
        set_cur(e);
    }
    changed = 1;
}

void replace_chars(byte c, word n) {
    word e = lend(cur), i;
    if (cur + n > e) return;
    if (c == CR) {                      // r Enter splits the line
        del(cur, n);
        ins_char(cur, CR);
        set_cur(cur + 1);
    } else {
        for (i = 0; i < n; i++) {
            del(cur + i, 1);
            ins_char(cur + i, c);
        }
        set_cur(cur + n - 1);
    }
    changed = 1;
}

void toggle_case(word n) {
    word e = lend(cur);
    byte c;
    while (n-- && cur < e) {
        c = ch(cur);
        if (c >= 'a' && c <= 'z') c -= 32;
        else if (c >= 'A' && c <= 'Z') c += 32;
        if (c != ch(cur)) {
            del(cur, 1);
            ins_char(cur, c);
        }
        if (cur + 1 < e) set_cur(cur + 1);
        else break;
    }
    changed = 1;
}

void file_info(void) {
    char b[40], *p = b;
    strcpy(p, modified ? " [Modified] " : " ");
    p += strlen(p);
    p = fmtnum(p, nlines);
    strcpy(p, " lines --");
    p = fmtnum(p + 9, (word)((unsigned long)(curline + 1) * 100 / nlines));
    strcpy(p, "%--");
    say("\"", fname[0] ? fname : "[No Name]");
    strcat(msg, "\"");
    strcat(msg, b);
}

// ^F ^B scroll a page (less two lines), ^D ^U n lines (half a screen).
void page(int dir, word n) {
    word step = n, nt, l;
    if (dir == 2 || dir == -2) step = (trows > 2 ? (word)(trows - 2) : 1) * n;
    if (dir > 0) {
        nt = topline + step < nlines ? topline + step : nlines - 1;
        l = dir == 2 ? (curline > nt ? curline : nt)
                     : (curline + step < nlines ? curline + step : nlines - 1);
    } else {
        nt = topline > step ? topline - step : 0;
        l = dir == -2 ? (curline < nt + trows - 1 ? curline : nt + trows - 1)
                      : (curline > step ? curline - step : 0);
    }
    if (nt == topline && l == curline) return;
    force_top = nt;
    set_cur(first_nb(line_pos(l)));
}

void substitute(char *s, word first, word last) {
    char delim = *s++, *old = s, *rep, *flags;
    word ol, rl, l, p, e, count = 0, lines = 0, at = cur;
    byte g, hit;
    rep = strchr(old, delim);
    if (!rep) {
        say("Usage: s/old/new/[g]", 0);
        return;
    }
    *rep++ = 0;
    flags = strchr(rep, delim);
    if (flags) *flags++ = 0;
    else flags = "";
    g = strchr(flags, 'g') != 0;
    ol = (word)strlen(old);
    rl = (word)strlen(rep);
    if (!ol) {
        if (!patlen) {
            say("No previous search", 0);
            return;
        }
        old = pat;
        ol = patlen;
    }
    p = line_pos(first);
    for (l = first; l <= last; l++) {
        hit = 0;
        for (;;) {
            e = lend(p);
            while (p + ol <= e) {
                word i;
                for (i = 0; i < ol && ch(p + i) == (byte)old[i]; i++) {}
                if (i == ol) break;
                p++;
            }
            if (p + ol > e) break;
            del(p, ol);
            ins(p, (byte *)rep, rl);
            at = p;
            p += rl;
            count++;
            hit = 1;
            if (!g) break;
        }
        if (hit) lines++;
        p = lend(p) + 1;
        if (p >= LEN) break;
    }
    if (!count) {
        say("Pattern not found: ", old);
        return;
    }
    set_cur(first_nb(lstart(at)));
    if (count > 1) {
        char b[40], *q = fmtnum(b, count);
        strcpy(q, " substitutions on ");
        q = fmtnum(q + 18, lines);
        strcpy(q, lines == 1 ? " line" : " lines");
        say(b, 0);
    }
    changed = 1;
}

// An ex address: a line number, . or $.  Sets *l (from 0).
byte address(char **pp, word *l) {
    char *p = *pp;
    word n = 0;
    if (*p == '.') {
        *l = curline;
        p++;
    } else if (*p == '$') {
        *l = nlines - 1;
        p++;
    } else if (*p >= '0' && *p <= '9') {
        while (*p >= '0' && *p <= '9') n = n * 10 + (word)(*p++ - '0');
        *l = n ? n - 1 : 0;
    } else {
        return 0;
    }
    *pp = p;
    return 1;
}

void ex(void) {
    char line[80], *p, *arg, cmd[8];
    byte n = 0, bang = 0, ranged = 0;
    word l, lines, bytes, first, last;
    if (read_line(':', line, sizeof line - 1) < 0) return;
    p = line;
    while (*p == ' ' || *p == ':') p++;
    if (!*p) return;
    // A range: N, ., $, %, or two of them with a comma.
    first = last = curline;
    if (*p == '%') {
        first = 0;
        last = nlines - 1;
        p++;
        ranged = 1;
    } else if (address(&p, &first)) {
        last = first;
        ranged = 1;
        if (*p == ',') {
            p++;
            if (!address(&p, &last)) {
                say("Bad range", 0);
                return;
            }
        }
    }
    if (first > last) {
        l = first;
        first = last;
        last = l;
    }
    if (last >= nlines) last = nlines - 1;
    while (*p == ' ') p++;
    if (!*p) {                          // :N goes to line N
        set_cur(first_nb(line_pos(last)));
        return;
    }
    if (*p == 's' && p[1] && !isalnum((byte)p[1])) {
        substitute(p + 1, first, last);
        return;
    }
    if (*p == 'd' && (!p[1] || p[1] == ' ')) {
        set_cur(line_pos(first));
        l = line_pos(last);
        l = lend(l) + 1;
        yank(cur, l - cur, 1);
        del(cur, l - cur);
        ensure_final_cr();
        if (cur >= LEN) set_cur(lstart(LEN - 1));
        set_cur(first_nb(lstart(cur)));
        changed = 1;
        return;
    }
    (void)ranged;
    while (isalpha((byte)*p) && n < sizeof cmd - 1) cmd[n++] = *p++;
    cmd[n] = 0;
    if (*p == '!') {
        bang = 1;
        p++;
    }
    while (*p == ' ') p++;
    arg = p;
    if (!strcmp(cmd, "w") || !strcmp(cmd, "wq") || !strcmp(cmd, "x")) {
        if (cmd[0] == 'x' && !modified && !*arg) finish();
        if (!write_file(*arg ? arg : fname)) return;
        if (cmd[1] == 'q' || cmd[0] == 'x') finish();
    } else if (!strcmp(cmd, "q")) {
        if (modified && !bang) say("No write since last change (add ! to override)", 0);
        else finish();
    } else if (!strcmp(cmd, "e")) {
        if (modified && !bang) {
            say("No write since last change (add ! to override)", 0);
        } else if (*arg || fname[0]) {
            if (*arg) strncpy(fname, arg, sizeof fname - 1);
            edit_file(fname);
        }
    } else if (!strcmp(cmd, "r")) {
        if (!*arg) {
            say("Usage: :r file", 0);
        } else {
            l = lend(cur) + 1;
            n = load(arg, l, &lines, &bytes);
            if (!n) say("Can't open ", arg);
            else {
                set_cur(l < LEN ? l : lstart(LEN - 1));
                say_file(arg, lines, bytes, "");
                changed = 1;
            }
        }
    } else if (!strcmp(cmd, "redraw")) {
        clear_screen();
    } else if (!strcmp(cmd, "h") || !strcmp(cmd, "help")) {
        say(":w :q :q! :wq :x :e file :r file :N :s/a/b/g :%s/a/b/g  (man vi)", 0);
    } else {
        say("Not an editor command: ", line);
    }
}

void normal(void) {
    word c, count = 0, n, t;
    byte hc = 0, from_replay = rp_i < rp_n;
    if (!from_replay) rp_n = rp_i = 0;
    rec_n = 0;
    rec_over = 0;
    changed = 0;
    keep_want = 0;
    useq++;
    c = getkey(1);
    msg[0] = 0;
    while ((c >= '1' && c <= '9') || (count && c == '0')) {
        count = count * 10 + (c - '0');
        c = getkey(1);
    }
    hc = count != 0;
    n = hc ? count : 1;
    switch (c) {
    case 'i': insert_mode(n); break;
    case 'a':
        if (ch(cur) != CR) set_cur(cur + 1);
        insert_mode(n);
        break;
    case 'I': set_cur(first_nb(lstart(cur))); insert_mode(n); break;
    case 'A': set_cur(lend(cur)); insert_mode(n); break;
    case 'o': case 'O':
        t = c == 'o' ? lend(cur) + 1 : lstart(cur);
        ins_char(t, CR);
        set_cur(t);
        insert_mode(1);
        break;
    case 'x': case 'X':
        if (c == 'x') {
            t = lend(cur);
            if (cur == t) break;
            if (t - cur > n) t = cur + n;
            yank(cur, t - cur, 0);
            del(cur, t - cur);
        } else {
            t = lstart(cur);
            if (cur == t) break;
            if (cur - t > n) t = cur - n;
            yank(t, cur - t, 0);
            del(t, cur - t);
        }
        changed = 1;
        break;
    case 'd': case 'c': case 'y': case '>': case '<':
        operator((byte)c, n, hc, 0);
        break;
    case 'D': operator('d', n, hc, '$'); break;
    case 'C': operator('c', n, hc, '$'); break;
    case 's': operator('c', n, hc, 'l'); break;
    case 'S': operator('c', n, hc, 'c'); break;
    case 'Y': operator('y', n, hc, 'y'); break;
    case 'p': put(1, n); break;
    case 'P': put(0, n); break;
    case 'J': join(n); break;
    case 'r':
        t = getkey(0);
        if (t != ESC) replace_chars((byte)t, n);
        break;
    case '~': toggle_case(n); break;
    case 'u': undo(); break;
    case '.':
        if (dot_n) {
            memcpy(replay, dot, dot_n);
            rp_n = dot_n;
            rp_i = 0;
        }
        break;
    case ':': ex(); break;
    case 'Z':
        t = getkey(0);
        if (t == 'Z') {
            if (!modified || write_file(fname)) finish();
        } else if (t == 'Q') {
            finish();
        }
        break;
    case 7: file_info(); break;                     // ^G
    case 12:                                        // ^L, or up
        if (raw_seen) clear_screen();
        else if (motion(c, n, hc, 0, &t)) set_cur(t);
        break;
    case 6: case K_PGDN: page(2, n); break;         // ^F
    case 2: case K_PGUP: page(-2, n); break;        // ^B
    case 4: page(1, hc ? n : trows / 2); break;     // ^D
    case 21: page(-1, hc ? n : trows / 2); break;   // ^U
    case ESC: break;
    default:
        if (motion(c, n, hc, 0, &t)) set_cur(t);
        break;
    }
    if (changed && !from_replay && !rec_over) {
        memcpy(dot, rec, rec_n);
        dot_n = rec_n;
    }
    ensure_final_cr();
    if (cur >= LEN) set_cur(lstart(LEN - 1));
    if (ch(cur) == CR && cur > lstart(cur)) set_cur(cur - 1);
    if (!keep_want) want = vcol(cur);
}

// ── Main ───────────────────────────────────────────────────────────

void finish(void) {
    ostr("\033[r\033[0m");
    ogoto(rows, 1);
    ostr("\033[K");
    oflush();
    plat_done();
    plat_exit(0);
}

word number(const char *s) {
    word n = 0;
    while (*s >= '0' && *s <= '9') n = n * 10 + (word)(*s++ - '0');
    return n;
}

void usage(void) {
    ostr("Usage: vi [-w cols] [-h rows] [file]\r\n");
    oflush();
    plat_exit(1);
}

int main(int argc, char **argv) {
    word size = 0xC000, v, s;
    byte *m;
    int ai;
    byte set_size = 0;
    for (ai = 1; ai < argc && argv[ai][0] == '-'; ai++) {
        if (ai + 1 >= argc) usage();
        v = number(argv[ai + 1]);
        if (argv[ai][1] == 'w' && v >= 20 && v <= MAXCOLS) cols = (byte)v;
        else if (argv[ai][1] == 'h' && v >= 4 && v <= MAXROWS) rows = (byte)v;
        else usage();
        set_size = 1;
        ai++;
    }
    if (ai < argc - 1) usage();
    if (ai < argc) strncpy(fname, argv[ai], sizeof fname - 1);
    if (!set_size) plat_size(&rows, &cols);
    trows = rows - 1;

    m = plat_alloc(&size);
    if (!m || size < 8192) {
        ostr("vi: not enough memory\r\n");
        oflush();
        plat_exit(1);
    }
    shadow = m;                         // carve it up
    s = (word)rows * cols;
    m += s;
    size -= s;
    ucap = size / 8;
    ubuf = m;
    m += ucap;
    ycap = size / 5;
    ybuf = m;
    m += ycap;
    buf = m;
    bufsize = size - ucap - ycap;

    plat_init();
    edit_file(fname);
    clear_screen();
    ostr("\033[1;");                    // the text scrolls above the bottom line
    onum(trows);
    ochar('r');
    for (;;) normal();
    return 0;
}
