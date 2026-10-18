// term.c -- the console of the FujiNet C programs (see term.h).
//
// Everything written goes through a copy of the screen, a byte per cell
// with bit 7 set where it is in reverse video.  term_invert_row() flips a
// row in the copy and sends it again; that is how the CoCo programs these
// come from highlight a row, by flipping its attributes in screen memory.
//
// The copy wraps at 80 columns.  When a line is full, the next character
// is sent after a CR LF, not left to the terminal, which may be wider; the
// scroll region is the first 24 rows, which the terminal may also exceed.
//
// The keys come from /term, read raw.  The tether sends the arrow keys and
// Page Up/Down as 128..133, which the TFR/911H firmware gives a program as
// they are while bit 7 of $FF42 is set; ^C and ^E arrive as signals.

#include "term.h"

#ifndef _CMOC_VERSION_
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <sys/select.h>
#endif

// ── Platform: NitrOS-9 ─────────────────────────────────────────────

#ifdef _CMOC_VERSION_

static byte term;           // our path to /term, raw
static byte sig;            // the last signal (^C, ^E)
static byte old_ff42;

#define FF42 (*(byte *)0xFF42)

asm __norts__ void intercept(void) {
    asm {
        stb     ,u
        rti
    }
}

static byte os9_read(byte path, byte *buf, word n) {
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
    return got != 0;
}

static void plat_init(void) {
    byte opts[32];
    byte path = 255;
    byte *flag = &sig;
    void (*handler)(void) = intercept;
    static char name[] = "/term\r";
    char *n = name;
    asm {
        ldx     :handler
        pshs    u
        ldu     :flag
        os9     $09             ; F$Icpt
        puls    u
        lda     #3              ; UPDAT.
        ldx     :n
        os9     $84             ; I$Open
        bcs     @err
        sta     :path
@err
    }
    term = path == 255 ? 0 : path;
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

static void plat_done(void) {
    byte path = term;
    FF42 = old_ff42 & 0x7F;
    if (path) {
        asm {
            lda     :path
            os9     $8F         ; I$Close
        }
    }
}

static void plat_write(const char *p, word n) {
    asm {
        lda     #1              ; standard output
        ldx     :p
        pshs    y
        ldy     :n
        os9     $8A             ; I$Write
        puls    y
    }
}

static byte plat_ready(void) {
    byte path = term, ok = 0;
    if (sig) return 1;
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

// ^C (S$Intrpt) and ^E (S$Abort) end a wait for a key, as BREAK.
static byte plat_getc(void) {
    byte c = 0;
    for (;;) {
        if (sig) {
            sig = 0;
            return KEY_BREAK;
        }
        if (os9_read(term, &c, 1)) return c;
    }
}

void term_sleep(word ticks) {
    term_flush();
    if (!ticks) return;         // F$Sleep 0 would sleep until a signal
    asm {
        ldx     :ticks
        os9     $0A             ; F$Sleep
    }
}

dword term_seconds(void) {
    byte t[6];
    asm {
        leax    :t
        os9     $15             ; F$Time: y m d h m s
    }
    return (dword)t[3] * 3600 + (word)t[4] * 60 + t[5];
}

// ── Platform: Unix (for testing) ───────────────────────────────────

#else

static struct termios saved_tio;
static int pending = -1;

static void plat_init(void) {
    struct termios t;
    tcgetattr(0, &saved_tio);
    t = saved_tio;
    cfmakeraw(&t);
    tcsetattr(0, TCSANOW, &t);
}

static void plat_done(void) { tcsetattr(0, TCSANOW, &saved_tio); }

static void plat_write(const char *p, word n) {
    if (write(1, p, n) < 0) exit(1);
}

static int wait_input(int ms) {
    fd_set f;
    struct timeval tv;
    FD_ZERO(&f);
    FD_SET(0, &f);
    tv.tv_sec = 0;
    tv.tv_usec = ms * 1000;
    return select(1, &f, 0, 0, &tv) > 0;
}

static byte plat_ready(void) { return pending >= 0 || wait_input(0); }

static int rawc(void) {
    unsigned char c;
    if (read(0, &c, 1) != 1) {
        term_done();
        exit(1);
    }
    return c;
}

// The terminal's arrow and page keys become the tether's codes.
static byte plat_getc(void) {
    int c, d, n = 0;
    char seq[8];
    if (pending >= 0) {
        c = pending;
        pending = -1;
        return (byte)c;
    }
    c = rawc();
    if (c != 27 || !wait_input(50)) return (byte)c;
    d = rawc();
    if (d != '[' && d != 'O') {
        pending = d;
        return 27;
    }
    while (n < 7) {
        seq[n++] = (char)(c = rawc());
        if (isalpha(c) || c == '~') break;
    }
    seq[n] = 0;
    if (!strcmp(seq, "A")) return 128;
    if (!strcmp(seq, "B")) return 129;
    if (!strcmp(seq, "D")) return 130;
    if (!strcmp(seq, "C")) return 131;
    if (!strcmp(seq, "5~")) return 132;
    if (!strcmp(seq, "6~")) return 133;
    return 0;
}

void term_sleep(word ticks) {
    term_flush();
    usleep((useconds_t)ticks * 16667);
}

dword term_seconds(void) {
    time_t now = time(0);
    struct tm *tm = localtime(&now);
    return (dword)tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
}

char *strupr(char *s) {
    char *p;
    for (p = s; *p; p++) *p = (char)toupper((unsigned char)*p);
    return s;
}

char *strlwr(char *s) {
    char *p;
    for (p = s; *p; p++) *p = (char)tolower((unsigned char)*p);
    return s;
}

#endif

// ── The screen ─────────────────────────────────────────────────────

static byte scr[TERM_ROWS][TERM_COLS];  // each cell; bit 7: reverse video
static byte cx, cy;         // the cursor
static byte wrap;           // the row is full: the next character starts a new one
static byte rv;             // 0x80 if what follows is in reverse video
static byte out_rv;         // 0x80 if the terminal has reverse video on
static char obuf[256];
static word olen;

void term_flush(void) {
    if (olen) plat_write(obuf, olen);
    olen = 0;
}

static void out(char c) {
    obuf[olen++] = c;
    if (olen == sizeof obuf) term_flush();
}

static void outs(const char *s) {
    while (*s) out(*s++);
}

static void out_num(byte n) {
    if (n >= 10) out_num(n / 10);
    out((char)('0' + n % 10));
}

static void out_rv_set(byte r) {
    if (r == out_rv) return;
    outs(r ? "\033[7m" : "\033[27m");
    out_rv = r;
}

static void out_goto(byte x, byte y) {
    outs("\033[");
    out_num(y + 1);
    out(';');
    out_num(x + 1);
    out('H');
}

// The copy's half of a new line (the terminal does its own).
static void next_row(void) {
    cx = 0;
    wrap = 0;
    if (cy < TERM_ROWS - 1) {
        cy++;
        return;
    }
    memmove(scr[0], scr[1], (TERM_ROWS - 1) * TERM_COLS);
    memset(scr[TERM_ROWS - 1], ' ', TERM_COLS);
}

void term_putc(char c) {
    byte ch = (byte)c;
    if (ch == '\n') {
        out('\r');
        out('\n');
        next_row();
        return;
    }
    if (ch == '\r') {
        out('\r');
        cx = 0;
        wrap = 0;
        return;
    }
    if (ch == '\b') {
        if (wrap) wrap = 0;
        else if (cx) cx--;
        out_goto(cx, cy);
        return;
    }
    if (ch == '\a') {
        out(7);
        return;
    }
    if (ch == '\t') {
        do term_putc(' '); while (cx & 7);
        return;
    }
    if (ch < 32) return;
    if (ch > 126) ch = '?';
    if (wrap) {
        out('\r');
        out('\n');
        next_row();
    }
    out_rv_set(rv);
    out((char)ch);
    scr[cy][cx] = ch | rv;
    if (cx == TERM_COLS - 1) wrap = 1;
    else cx++;
}

void term_puts(const char *s) {
    while (*s) term_putc(*s++);
}

// term_printf() does its own formatting: CMOC's vsprintf() does not work
// in an OS-9 program (0.1.97: it writes to standard output, then the stack
// overflows).  It knows %s %c %d %u %x %X %%, with l, a width or *, and
// the flags - and 0, which is all the programs here use.

static int out_count;

static void out_field(const char *s, word len, int width, byte left, char pad) {
    int n = width - (int)len;
    out_count += len;
    if (!left) for (; n > 0; n--, out_count++) term_putc(pad);
    while (len--) term_putc(*s++);
    if (left) for (; n > 0; n--, out_count++) term_putc(' ');
}

int term_printf(const char *fmt, ...) {
    va_list ap;
    char num[12], *p, c;
    const char *s;
    byte left, lng, neg, base, d;
    char pad;
    int width;
    unsigned long u;
    out_count = 0;
    va_start(ap, fmt);
    while ((c = *fmt++) != 0) {
        if (c != '%') {
            term_putc(c);
            out_count++;
            continue;
        }
        left = lng = 0;
        pad = ' ';
        width = 0;
        if (*fmt == '-') {
            left = 1;
            fmt++;
        }
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        if (*fmt == '*') {
            width = va_arg(ap, int);
            fmt++;
            if (width < 0) {
                left = 1;
                width = -width;
            }
        } else {
            while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        }
        if (*fmt == 'l') {
            lng = 1;
            fmt++;
        }
        c = *fmt++;
        switch (c) {
        case 's':
            s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            out_field(s, (word)strlen(s), width, left, ' ');
            break;
        case 'c':
            num[0] = (char)va_arg(ap, int);
            out_field(num, 1, width, left, ' ');
            break;
        case 'd': case 'u': case 'x': case 'X':
            neg = 0;
            if (lng) {
                u = va_arg(ap, unsigned long);
                if (c == 'd' && (long)u < 0) {
                    neg = 1;
                    u = (unsigned long)-(long)u;
                }
            } else if (c == 'd') {
                long i = va_arg(ap, int);
                if (i < 0) {
                    neg = 1;
                    i = -i;
                }
                u = (unsigned long)i;
            } else {
                u = va_arg(ap, unsigned);
            }
            base = (c == 'd' || c == 'u') ? 10 : 16;
            p = num + sizeof num;
            *--p = 0;
            do {
                d = (byte)(u % base);
                *--p = (char)(d < 10 ? '0' + d : (c == 'x' ? 'a' : 'A') + d - 10);
                u /= base;
            } while (u);
            if (neg) {
                if (pad == '0' && !left) {
                    term_putc('-');
                    out_count++;
                    width--;
                } else {
                    *--p = '-';
                }
            }
            out_field(p, (word)strlen(p), width, left, pad);
            break;
        case 0:
            fmt--;
            break;
        default:        // %% and anything unknown
            term_putc(c);
            out_count++;
            break;
        }
    }
    va_end(ap);
    return out_count;
}

void term_gotoxy(byte x, byte y) {
    if (x >= TERM_COLS) x = TERM_COLS - 1;
    if (y >= TERM_ROWS) y = TERM_ROWS - 1;
    cx = x;
    cy = y;
    wrap = 0;
    out_goto(x, y);
}

byte term_wherex(void) { return cx; }
byte term_wherey(void) { return cy; }

void term_clrscr(void) {
    memset(scr, ' ', sizeof scr);
    outs("\033[0m\033[2J\033[H");
    out_rv = 0;
    cx = cy = 0;
    wrap = 0;
}

void term_clreol(void) {
    if (wrap) return;
    memset(&scr[cy][cx], ' ', TERM_COLS - cx);
    out_rv_set(0);
    outs("\033[K");
}

void term_reverse(byte on) { rv = on ? 0x80 : 0; }

void term_invert_row(byte y) {
    byte x, *p;
    if (y >= TERM_ROWS) return;
    p = scr[y];
    out_goto(0, y);
    for (x = 0; x < TERM_COLS; x++) {
        p[x] ^= 0x80;
        out_rv_set(p[x] & 0x80);
        out((char)(p[x] & 0x7F));
    }
    out_goto(cx, cy);
}

void term_cursor(byte on) { outs(on ? "\033[?25h" : "\033[?25l"); }

void term_bell(void) { out(7); }

void term_init(void) {
    plat_init();
    memset(scr, ' ', sizeof scr);
    olen = 0;
    rv = out_rv = 0;
    outs("\033[0m\033[1;24r\033[?25l");
    term_clrscr();
    term_flush();
}

void term_done(void) {
    outs("\033[0m\033[r\033[2J\033[H\033[?25h");
    term_flush();
    plat_done();
}

static byte map_key(byte c) {
    static const byte keys[6] = { KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_PGUP, KEY_PGDN };
    if (c >= 128 && c <= 133) return keys[c - 128];
    if (c == 127) return KEY_BS;
    if (c == 27) return KEY_BREAK;
    return c;
}

byte term_ready(void) {
    term_flush();
    return plat_ready();
}

byte term_getc(void) {
    term_flush();
    return map_key(plat_getc());
}

byte term_inkey(void) {
    return term_ready() ? term_getc() : 0;
}

void term_getline(char *buf, byte size) {
    byte x0 = cx, y0 = cy, i = 0, c;
    term_cursor(1);
    for (;;) {
        c = term_getc();
        if (c == KEY_BREAK) {
            i = 0;
            break;
        }
        if (c == KEY_ENTER) break;
        if (c == KEY_BS || c == KEY_LEFT) {
            if (i) {
                i--;
                term_gotoxy(x0 + i, y0);
                term_putc(' ');
                term_gotoxy(x0 + i, y0);
            }
        } else if (c >= 32 && c < 127 && i < size - 1 && x0 + i < TERM_COLS - 1) {
            buf[i++] = (char)c;
            term_putc((char)c);
        }
    }
    buf[i] = 0;
    term_cursor(0);
}
