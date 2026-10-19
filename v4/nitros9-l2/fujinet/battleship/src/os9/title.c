/*
  NitrOS-9: the logo, big, for the top of the screens that have it, and the
  title, with a battleship (shipart.py makes shipart.h), shown while
  fnbattleship starts.
  @license gpl v. 3, see fujinet/LICENSE for details.
*/

#include "../misc.h"
#include "shipart.h"

#define WHITE  (TC_BRIGHT + TC_WHITE)
#define YELLOW (TC_BRIGHT + TC_YELLOW)

extern const char build_date[];
extern const char upstream_version[];

static void ink(byte fg, byte bg)
{
    term_color(fg, bg);
}

// The letters of the logo, five dots high, of the widths given; a row of
// dots is the low bits of a byte, the leftmost the highest.
static const char letters[] = "FUJIBATLESHP";
static const byte widths[12] = {4, 4, 4, 3, 4, 4, 5, 4, 4, 4, 4, 4};
static const byte font[12 * 5] = {
    0x0F, 0x08, 0x0E, 0x08, 0x08,   // F
    0x09, 0x09, 0x09, 0x09, 0x06,   // U
    0x03, 0x01, 0x01, 0x09, 0x06,   // J
    0x07, 0x02, 0x02, 0x02, 0x07,   // I
    0x0E, 0x09, 0x0E, 0x09, 0x0E,   // B
    0x06, 0x09, 0x0F, 0x09, 0x09,   // A
    0x1F, 0x04, 0x04, 0x04, 0x04,   // T
    0x08, 0x08, 0x08, 0x08, 0x0F,   // L
    0x0F, 0x08, 0x0E, 0x08, 0x0F,   // E
    0x07, 0x08, 0x06, 0x01, 0x0E,   // S
    0x09, 0x09, 0x0F, 0x09, 0x09,   // H
    0x0E, 0x09, 0x0E, 0x08, 0x08,   // P
};

#define LOGO_X 4                // "FUJI BATTLESHIP" is 71 columns wide
#define LOGO_W 71

// "FUJI BATTLESHIP" on rows y to y + 2, and waves under it.
static void logo(byte y)
{
    static const char text[] = "FUJI BATTLESHIP";
    static const byte fuji[3] = {YELLOW, YELLOW, TC_YELLOW};
    static const byte ship[3] = {WHITE, TC_WHITE, TC_CYAN};
    static const byte dots[4] = {' ', G_LOWER, G_UPPER, G_FULL};
    byte r, k, b, top, bottom, x;
    const char *p;

    for (r = 0; r < 3; r++) {
        term_gotoxy(LOGO_X, y + r);
        for (p = text; *p; p++) {
            if (*p == ' ') {
                ink(TC_DEFAULT, TC_DEFAULT);
                term_puts("   ");
                continue;
            }
            for (k = 0; letters[k] != *p; k++)
                ;
            top = font[k * 5 + r * 2];
            bottom = r < 2 ? font[k * 5 + r * 2 + 1] : 0;
            ink(p < text + 4 ? fuji[r] : ship[r], TC_DEFAULT);
            for (b = (byte)(1 << (widths[k] - 1)); b; b >>= 1)
                term_putc((char)dots[(top & b ? 2 : 0) | (bottom & b ? 1 : 0)]);
            if (p[1] && p[1] != ' ')
                term_putc(' ');
        }
    }
    term_gotoxy(LOGO_X, y + 3);
    for (x = 0; x < LOGO_W; x++) {
        ink(x % 7 == 3 ? TC_BRIGHT + TC_CYAN : x & 1 ? TC_BRIGHT + TC_BLUE : TC_BLUE,
            TC_DEFAULT);
        term_putc((char)G_WAVES);
    }
}

void drawLogo()
{
    logo(0);
}

// ── The title ──────────────────────────────────────────────────────

// The palette of shipart.py: a letter to a color.
static byte color_of(char c)
{
    switch (c) {
    case 'W': return WHITE;
    case 'w': return TC_WHITE;
    case 'B': return TC_BRIGHT + TC_BLUE;
    case 'b': return TC_BLUE;
    case 'C': return TC_BRIGHT + TC_CYAN;
    case 'c': return TC_CYAN;
    case 'R': return TC_BRIGHT + TC_RED;
    case 'r': return TC_RED;
    case 'Y': return YELLOW;
    case 'y': return TC_YELLOW;
    }
    return 0;
}

// A cell of two pixels, of colors t (top) and b (bottom), 0 for none.  A
// background can only be one of the eight plain colors, so where both are
// bright, the bottom one is drawn plain.
static void pixels(byte t, byte b)
{
    if (!t && !b) {
        ink(TC_DEFAULT, TC_DEFAULT);
        term_putc(' ');
    } else if (t == b) {
        ink(t, TC_DEFAULT);
        term_putc((char)G_FULL);
    } else if (!b) {
        ink(t, TC_DEFAULT);
        term_putc((char)G_UPPER);
    } else if (!t) {
        ink(b, TC_DEFAULT);
        term_putc((char)G_LOWER);
    } else if (b <= TC_WHITE) {
        ink(t, b);
        term_putc((char)G_UPPER);
    } else if (t <= TC_WHITE) {
        ink(b, t);
        term_putc((char)G_LOWER);
    } else {
        ink(t, b - TC_BRIGHT);
        term_putc((char)G_UPPER);
    }
}

static void art(byte x, byte y)
{
    const char *top = ship_art, *bottom;
    byte r, c;

    for (r = 0; r < SHIP_H; r += 2) {
        term_gotoxy(x, y + r / 2);
        bottom = top + SHIP_W;
        for (c = 0; c < SHIP_W; c++)
            pixels(color_of(top[c]), color_of(bottom[c]));
        top = bottom + SHIP_W;
    }
}

static void line(byte y, byte fg, const char *s)
{
    term_gotoxy((byte)((WIDTH - strlen(s)) / 2), y);
    ink(fg, TC_DEFAULT);
    term_puts(s);
}

// Shown as fnbattleship starts, for three seconds or until a key.
void title(void)
{
    byte i;

    term_clrscr();
    logo(1);
    art(4, 6);
    line(17, TC_BRIGHT + TC_CYAN, "Sink their fleets before they sink yours: up to four players");
    line(18, TC_WHITE, "by Eric Carr, Thomas Cherryhomes, and FujiNet's friends");
    line(19, TC_WHITE, "NitrOS-9 port by DILLER, for the TFR/911H");
    term_gotoxy(22, 21);
    ink(TC_WHITE, TC_DEFAULT);
    term_puts("version ");
    ink(WHITE, TC_DEFAULT);
    term_puts(upstream_version);
    ink(TC_WHITE, TC_DEFAULT);
    term_puts(", built ");
    ink(WHITE, TC_DEFAULT);
    term_puts(build_date);
    ink(TC_DEFAULT, TC_DEFAULT);
    for (i = 0; i < 30 && !term_ready(); i++)
        term_sleep(6);
    while (term_ready())
        term_getc();
}
