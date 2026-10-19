/**
 * @brief   fnweather: the icons, and the big digits
 * @license gpl v. 3
 * @verbose Pixel art, two pixels to a character cell: the top one drawn
 *          with an upper half block in its color, the bottom one with a
 *          lower half block, or as the background where both are
 *          colored.  The icons are made by icons.py, from shapes (a sun,
 *          clouds of overlapping discs, drops, flakes, a bolt, bars of
 *          fog), into icondata.h.
 */

#include "weather.h"
#include "icondata.h"

// The palette of icons.py: a letter to a color.
static byte color_of(char c)
{
    switch (c) {
    case 'Y': return TC_BRIGHT + TC_YELLOW;
    case 'y': return TC_YELLOW;
    case 'W': return TC_BRIGHT + TC_WHITE;
    case 'w': return TC_WHITE;
    case 'B': return TC_BRIGHT + TC_BLUE;
    case 'b': return TC_BLUE;
    case 'C': return TC_BRIGHT + TC_CYAN;
    case 'c': return TC_CYAN;
    case 'R': return TC_BRIGHT + TC_RED;
    }
    return 0;
}

// A cell of two pixels, of colors t (top) and b (bottom), 0 for none.  A
// background can only be one of the eight plain colors, so where both are
// bright, the bottom one is drawn plain.
static void cell(byte t, byte b)
{
    if (!t && !b) {
        term_color(TC_DEFAULT, TC_DEFAULT);
        term_putc(' ');
    } else if (t == b) {
        term_color(t, TC_DEFAULT);
        term_putc((char) TG_FULL);
    } else if (!b) {
        term_color(t, TC_DEFAULT);
        term_putc((char) TG_UPPERHALF);
    } else if (!t) {
        term_color(b, TC_DEFAULT);
        term_putc((char) TG_LOWERHALF);
    } else if (b <= TC_WHITE) {
        term_color(t, b);
        term_putc((char) TG_UPPERHALF);
    } else if (t <= TC_WHITE) {
        term_color(b, t);
        term_putc((char) TG_LOWERHALF);
    } else {
        term_color(t, b - TC_BRIGHT);
        term_putc((char) TG_UPPERHALF);
    }
}

// Big: 14 columns by 7 rows; small: 8 by 4.
void draw_icon(byte x, byte y, byte icon, byte big)
{
    byte w = big ? BIG_W : SMALL_W, h = big ? BIG_H : SMALL_H, r, c;
    const char *top, *bottom;

    // (word): CMOC keeps a byte times a byte constant to a byte.
    top = big ? big_icons + (word) icon * (BIG_W * BIG_H)
              : small_icons + (word) icon * (SMALL_W * SMALL_H);
    for (r = 0; r < h; r += 2) {
        term_gotoxy(x, y + r / 2);
        bottom = top + w;
        for (c = 0; c < w; c++)
            cell(color_of(top[c]), color_of(bottom[c]));
        top = bottom + w;
    }
    term_color(TC_DEFAULT, TC_DEFAULT);
}

// Digits five dots square, as the bounce-world title's letters are, and
// '-', 'C', 'F', and '*' for the degree sign, which is four dots wide.
static const char font_chars[] = "0123456789-CF*";
static const byte font[14][5] = {
    { 0x0E, 0x11, 0x11, 0x11, 0x0E },   // 0
    { 0x04, 0x0C, 0x04, 0x04, 0x0E },   // 1
    { 0x1E, 0x01, 0x0E, 0x10, 0x1F },   // 2
    { 0x1E, 0x01, 0x0E, 0x01, 0x1E },   // 3
    { 0x12, 0x12, 0x1F, 0x02, 0x02 },   // 4
    { 0x1F, 0x10, 0x1E, 0x01, 0x1E },   // 5
    { 0x0E, 0x10, 0x1E, 0x11, 0x0E },   // 6
    { 0x1F, 0x01, 0x02, 0x04, 0x04 },   // 7
    { 0x0E, 0x11, 0x0E, 0x11, 0x0E },   // 8
    { 0x0E, 0x11, 0x0F, 0x01, 0x0E },   // 9
    { 0x00, 0x00, 0x0E, 0x00, 0x00 },   // -
    { 0x0F, 0x10, 0x10, 0x10, 0x0F },   // C
    { 0x1F, 0x10, 0x1E, 0x10, 0x10 },   // F
    { 0x0C, 0x12, 0x0C, 0x00, 0x00 },   // degree sign, in the high bits
};

static int font_index(char ch)
{
    const char *p = font_chars;
    for (; *p; p++)
        if (*p == ch) return (int)(p - font_chars);
    return -1;
}

static byte glyph_width(char ch)
{
    return ch == '*' ? 4 : 5;
}

byte big_text_width(const char *s)
{
    byte w = 0;
    for (; *s; s++)
        w += glyph_width(*s) + 1;
    return w ? w - 1 : 0;
}

void draw_big_text(byte x, byte y, const char *s, byte fg)
{
    static const byte dots[4] = { ' ', TG_LOWERHALF, TG_UPPERHALF, TG_FULL };
    byte r, b, top, bottom, w;
    int k;
    const char *p;

    for (r = 0; r < 3; r++) {
        term_gotoxy(x, y + r);
        term_color(fg, TC_DEFAULT);
        for (p = s; *p; p++) {
            k = font_index(*p);
            w = glyph_width(*p);
            top = k < 0 ? 0 : font[k][r * 2];
            bottom = k < 0 || r == 2 ? 0 : font[k][r * 2 + 1];
            for (b = 0x10; b > (0x10 >> w); b >>= 1)
                term_putc((char) dots[(top & b ? 2 : 0) | (bottom & b ? 1 : 0)]);
            if (p[1]) term_putc(' ');
        }
    }
    term_color(TC_DEFAULT, TC_DEFAULT);
}
