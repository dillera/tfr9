// screen.c -- Texas Hold'em's drawing (see screen.h).  term.c writes
// straight to the console's 6850, some 4K a second, so bytes count.  Two
// copies of the screen are kept, a character and two colours a cell: what
// is wanted, which the game draws into, and what the terminal shows.
// scr_present() goes over the rows drawn in since the last time and sends
// only the cells that differ.  As synack's screen.c does, it sends a
// colour only when a character needs it changed, and both changes in one
// escape sequence; it erases the blank end of a row with ESC[K (the
// terminal erases in the background colour of the moment, as xterm's do)
// rather than with spaces; and it steps over cells that are already right
// with a cursor movement, or by sending them again when that is shorter.
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#include "screen.h"

#define CELLS (SCR_COLS * SCR_ROWS)

static byte want_ch[CELLS], want_fg[CELLS], want_bg[CELLS];  // what is drawn
static byte term_ch[CELLS], term_fg[CELLS], term_bg[CELLS];  // what is shown
static byte dirty[SCR_ROWS];    // a row drawn in since it was last sent

static byte ink_fg, ink_bg;

static byte cur_x, cur_y;       // the terminal's cursor; cur_x 255 if not known
static word cur_fg, cur_bg;     // the terminal's colours; 256 if not known

static char buf[200];
static byte len;

static void out(char c)
{
    if (len == sizeof buf - 1) {
        buf[len] = 0;
        term_raw(buf);
        len = 0;
    }
    buf[len++] = c;
}

static void outs(const char *s)
{
    while (*s)
        out(*s++);
}

static void outn(byte n)
{
    if (n >= 100)
        out((char)('0' + n / 100));
    if (n >= 10)
        out((char)('0' + n / 10 % 10));
    out((char)('0' + n % 10));
}

// The UTF-8 of a G_ code: four bytes a code in GLYPH_UTF8, padded with NULs.
static const char *glyph(byte ch)
{
    return GLYPH_UTF8 + (word)(byte)(ch - 0x80) * 4;
}

// The bytes a character takes.
static byte width(byte ch)
{
    const char *g;
    byte n;
    if (ch < 0x80)
        return 1;
    g = glyph(ch);
    for (n = 0; n < 4 && g[n]; n++)
        ;
    return n;
}

static void put_char(byte ch)
{
    const char *g;
    byte n;
    if (ch < 0x80) {
        out((char)ch);
        return;
    }
    g = glyph(ch);
    for (n = 0; n < 4 && g[n]; n++)
        out(g[n]);
}

// One escape sequence for the colours that differ.  A space shows only
// its background, and a full block only its foreground: the other is let
// be.
static void colours(byte fg, byte bg, byte ch)
{
    byte f = ch != ' ' && cur_fg != fg;
    byte b = ch != G_FULL && cur_bg != bg;
    if (!f && !b)
        return;
    outs("\033[");
    if (f) {
        outs("38;5;");
        outn(fg);
        cur_fg = fg;
    }
    if (b) {
        if (f)
            out(';');
        outs("48;5;");
        outn(bg);
        cur_bg = bg;
    }
    out('m');
}

// A cell is right on the terminal when it shows the same (a space in any
// foreground, a full block on any background).  term_ch[i] is read into
// a variable first: CMOC 0.1.97 compiled "ch == term_ch[i]" as LDD i,
// then LDB ch, then CMPB D,X, which spoils the index with ch.
static byte same(word i)
{
    byte ch = want_ch[i], t = term_ch[i];
    return ch == t
        && (ch == G_FULL || want_bg[i] == term_bg[i])
        && (ch == ' ' || want_fg[i] == term_fg[i]);
}

// A cell that can be sent with the colours the terminal has.
static byte cheap(word i)
{
    byte ch = want_ch[i];
    return (ch == G_FULL || want_bg[i] == cur_bg)
        && (ch == ' ' || want_fg[i] == cur_fg);
}

static void send(word i)
{
    byte ch = want_ch[i];
    colours(want_fg[i], want_bg[i], ch);
    put_char(ch);
    term_ch[i] = ch;
    term_fg[i] = want_fg[i];
    term_bg[i] = want_bg[i];
}

// The cursor to column x of row y, which starts at cell base.
static void go(byte x, byte y, word base)
{
    byte gap, cost, n;
    if (cur_x != 255 && cur_y == y && cur_x <= x) {
        gap = (byte)(x - cur_x);
        if (!gap)
            return;
        // The cells between are right already: sent again, if that is
        // no longer than ESC[nC and needs no colour.
        cost = 0;
        for (n = cur_x; n < x && cost <= 5; n++) {
            if (!cheap(base + n))
                cost = 255;
            else
                cost = (byte)(cost + width(want_ch[base + n]));
        }
        if (cost <= (gap < 10 ? 4 : 5)) {
            for (n = cur_x; n < x; n++)
                put_char(want_ch[base + n]);
        } else {
            outs("\033[");
            if (gap > 1)
                outn(gap);
            out('C');
        }
    } else if (cur_x != 255 && x == 0 && (byte)(cur_y + 1) == y) {
        out('\r');
        out('\n');
    } else {
        outs("\033[");
        outn((byte)(y + 1));
        if (x) {
            out(';');
            outn((byte)(x + 1));
        }
        out('H');
    }
    cur_x = x;
    cur_y = y;
}

void scr_present(void)
{
    byte x, y, t, el, b;
    word base, i;
    for (y = 0; y < SCR_ROWS; y++) {
        if (!dirty[y])
            continue;
        dirty[y] = 0;
        base = (word)y * SCR_COLS;
        // The blank end of the row, all spaces in one background
        b = want_bg[base + SCR_COLS - 1];
        for (t = SCR_COLS; t && want_ch[base + t - 1] == ' ' && want_bg[base + t - 1] == b; t--)
            ;
        el = 0;
        if (t <= SCR_COLS - 3)
            for (x = t; x < SCR_COLS; x++)
                if (!same(base + x)) {
                    el = 1;
                    break;
                }
        for (x = 0; x < (el ? t : SCR_COLS); x++) {
            i = base + x;
            if (same(i))
                continue;
            go(x, y, base);
            send(i);
            cur_x = x == SCR_COLS - 1 ? 255 : (byte)(x + 1);
        }
        if (el) {
            go(t, y, base);
            colours(0, b, ' ');
            outs("\033[K");
            for (x = t; x < SCR_COLS; x++) {
                i = base + x;
                term_ch[i] = ' ';
                term_fg[i] = 0;
                term_bg[i] = b;
            }
        }
    }
    if (len) {
        buf[len] = 0;
        term_raw(buf);
        len = 0;
    }
    term_flush();
}

void scr_clear(byte bg)
{
    word i;
    for (i = 0; i < CELLS; i++) {
        want_ch[i] = term_ch[i] = ' ';
        want_fg[i] = term_fg[i] = 0;
        want_bg[i] = term_bg[i] = bg;
    }
    for (i = 0; i < SCR_ROWS; i++)
        dirty[i] = 0;
    outs("\033[0m");
    cur_fg = cur_bg = 256;
    colours(0, bg, ' ');
    outs("\033[2J");
    cur_x = 255;
    scr_present();
}

void scr_init(byte bg)
{
    term_init();
    scr_clear(bg);
}

void scr_done(void)
{
    term_done();
}

void scr_ink(byte fg, byte bg)
{
    ink_fg = fg;
    ink_bg = bg;
}

void scr_put(byte x, byte y, byte ch)
{
    word i;
    if (x >= SCR_COLS || y >= SCR_ROWS)
        return;
    i = (word)y * SCR_COLS + x;
    want_ch[i] = ch;
    want_fg[i] = ch == ' ' ? 0 : ink_fg;
    if (ink_bg != C_KEEP)
        want_bg[i] = ink_bg;
    dirty[y] = 1;
}

byte scr_text(byte x, byte y, const char *s)
{
    while (*s)
        scr_put(x++, y, (byte)*s++);
    return x;
}

void scr_fill(byte x, byte y, byte w, byte h, byte ch)
{
    byte n;
    for (; h; h--, y++)
        for (n = 0; n < w; n++)
            scr_put((byte)(x + n), y, ch);
}

byte scr_bg_at(byte x, byte y)
{
    if (x >= SCR_COLS || y >= SCR_ROWS)
        return 0;
    return want_bg[(word)y * SCR_COLS + x];
}
