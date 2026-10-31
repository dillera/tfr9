// screen.c -- SYN-ACK WIRE's drawing (see screen.h).  What is drawn is
// collected here and handed to term.c, which writes it straight to the
// console's 6850, some 4K a second.  So bytes count: scr_ink() and
// scr_bold() only say what the next character wants, and one escape
// sequence brings the terminal to it, with only what changed; ends of rows
// are erased by the terminal (which erases in the background colour of
// the moment, as xterm's do), not padded with spaces.
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#include "term.h"
#include "screen.h"

static char buf[200];
static unsigned char len;
static unsigned char cur_fg, cur_bg, cur_bold;      // the terminal's
static unsigned char want_fg, want_bg, want_bold;   // the next character's

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

static void outn(unsigned char n)
{
    if (n >= 100)
        out((char)('0' + n / 100));
    if (n >= 10)
        out((char)('0' + n / 10 % 10));
    out((char)('0' + n % 10));
}

static void colour(unsigned char base, unsigned char c)
{
    if (c == C_NONE) {
        out((char)('0' + base / 10));
        out('9');
    } else {
        outn(base);
        outs(";5;");
        outn(c);
    }
}

// One escape sequence for whatever differs.
void scr_sync(void)
{
    unsigned char sep = 0;
    if (want_fg == cur_fg && want_bg == cur_bg && want_bold == cur_bold)
        return;
    outs("\033[");
    if (want_bold != cur_bold) {
        outs(want_bold ? "1" : "22");
        sep = 1;
    }
    if (want_fg != cur_fg) {
        if (sep)
            out(';');
        colour(38, want_fg);
        sep = 1;
    }
    if (want_bg != cur_bg) {
        if (sep)
            out(';');
        colour(48, want_bg);
    }
    out('m');
    cur_fg = want_fg;
    cur_bg = want_bg;
    cur_bold = want_bold;
}

// Spaces and erasing show only the background: bring that alone up to date.
static void sync_bg(void)
{
    if (want_bg == cur_bg)
        return;
    outs("\033[");
    colour(48, want_bg);
    out('m');
    cur_bg = want_bg;
}

void scr_flush(void)
{
    if (len) {
        buf[len] = 0;
        term_raw(buf);
        len = 0;
    }
    term_flush();
}

void scr_init(void)
{
    term_init();
    cur_fg = cur_bg = want_fg = want_bg = C_NONE;
    cur_bold = want_bold = 0;
}

void scr_done(void)
{
    scr_reset();
    scr_flush();
    term_done();
}

void scr_reset(void)
{
    outs("\033[0m");
    cur_fg = cur_bg = want_fg = want_bg = C_NONE;
    cur_bold = want_bold = 0;
}

void scr_clear(void)
{
    scr_reset();
    outs("\033[2J\033[H");
}

void scr_at(unsigned char row, unsigned char col)
{
    outs("\033[");
    outn((unsigned char)(row + 1));
    out(';');
    outn((unsigned char)(col + 1));
    out('H');
}

void scr_ink(unsigned char fg, unsigned char bg)
{
    want_fg = fg;
    want_bg = bg;
}

void scr_bold(unsigned char on)
{
    want_bold = on;
}

void scr_put(char c)
{
    scr_sync();
    if (c == '\\')
        outs("\xC2\xA5");       // the server's currency sign: the yen
    else
        out(c);
}

void scr_text(const char *s)
{
    while (*s)
        scr_put(*s++);
}

void scr_textn(const unsigned char *s, unsigned char n)
{
    while (n--)
        scr_put((char)*s++);
}

void scr_spaces(unsigned char n)
{
    if (!n)
        return;
    sync_bg();
    while (n--)
        out(' ');
}

void scr_glyph(const char *utf8)
{
    scr_sync();
    outs(utf8);
}

void scr_eol(void)
{
    sync_bg();
    outs("\033[K");
}

void scr_erase_row(void)
{
    sync_bg();
    outs("\033[2K");
}

void scr_raw(const char *s)
{
    outs(s);
}
