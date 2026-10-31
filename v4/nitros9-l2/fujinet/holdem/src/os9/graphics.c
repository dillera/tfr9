/*
  Platform specific graphics commands: upstream's drawing calls, on the
  tether's terminal, 80 columns by 24 in xterm's 256 colours.  Everything
  goes into screen.c's copy of the screen, and drawBuffer() sends what
  changed; with the double buffer off (disableDoubleBuffer()), each call
  is sent as it is drawn, for the animations.

  The screen is a card table: a dark bar at the top, the felt with its
  wooden rail round it, and the status bar at the foot (two rows: the
  messages or the moves, then the move's underline and the keys).  The
  felt is green, blue or red, as the C key cycles it (colorMode).
  @license gpl v. 3, see fujinet/LICENSE for details.
*/

#include "../misc.h"
#include "table.h"

unsigned char colorMode;
bool always_render_full_cards;
byte drawDirect;

// The row under the moves, as drawStatusTimer() left it: hideLine() puts
// back what the underline covered.
static byte keys_ch[WIDTH], keys_fg[WIDTH];

byte felt(void)
{
    return colorMode == 1 ? 24 : colorMode == 2 ? 88 : 28;
}

byte felt_line(void)
{
    return colorMode == 1 ? 31 : colorMode == 2 ? 124 : 34;
}

void show(void)
{
    if (drawDirect)
        scr_present();
}

static byte caps(byte c)
{
    return c >= 'a' && c <= 'z' ? (byte)(c - 32) : c;
}

byte drawCaps(byte x, byte y, const char *s, byte fg, byte max)
{
    scr_ink(fg, C_KEEP);
    for (; *s && max; s++, max--)
        scr_put(x++, y, caps((byte)*s));
    return x;
}

byte numberWidth(word n)
{
    byte w = 1;
    while (n >= 10) {
        n /= 10;
        w++;
    }
    return w;
}

byte drawNumber(byte x, byte y, word n, byte fg)
{
    byte w = numberWidth(n), i;
    scr_ink(fg, C_KEEP);
    for (i = w; i; i--) {
        scr_put((byte)(x + i - 1), y, (byte)('0' + n % 10));
        n /= 10;
    }
    return (byte)(x + w);
}

// ─── Cards ─────────────────────────────────────────────────────────

// The border of a card, in the ink of the moment
static void cardFrame(byte x, byte y)
{
    byte n;
    scr_put(x, y, G_ARC_UL);
    scr_put((byte)(x + 4), y, G_ARC_UR);
    scr_put(x, (byte)(y + 2), G_ARC_LL);
    scr_put((byte)(x + 4), (byte)(y + 2), G_ARC_LR);
    for (n = 1; n < 4; n++) {
        scr_put((byte)(x + n), y, G_HLINE);
        scr_put((byte)(x + n), (byte)(y + 2), G_HLINE);
    }
    scr_put(x, (byte)(y + 1), G_VLINE);
    scr_put((byte)(x + 4), (byte)(y + 1), G_VLINE);
}

void drawCardFace(byte x, byte y, const char *s)
{
    byte suit, n;
    if (!s) {
        // An empty place: its outline on the felt
        scr_ink(felt_line(), felt());
        cardFrame(x, y);
        scr_fill((byte)(x + 1), (byte)(y + 1), 3, 1, ' ');
        return;
    }
    // The border in the colour of the suit, or of the back: a colour
    // change fewer a row of a card
    if (s[0] == '?') {
        scr_ink(C_BACK, C_WHITE);
        cardFrame(x, y);
        for (n = 1; n < 4; n++)
            scr_put((byte)(x + n), (byte)(y + 1), G_QUAD_ULLR);
        return;
    }
    switch (s[1]) {
    case 'h': suit = G_HEART; break;
    case 'd': suit = G_DIAMOND; break;
    case 'c': suit = G_CLUB; break;
    default:  suit = G_SPADE; break;
    }
    scr_ink(suit == G_HEART || suit == G_DIAMOND ? C_RED : C_BLACK, C_WHITE);
    cardFrame(x, y);
    if (s[0] == 't') {
        scr_put((byte)(x + 1), (byte)(y + 1), '1');
        scr_put((byte)(x + 2), (byte)(y + 1), '0');
    } else {
        scr_put((byte)(x + 1), (byte)(y + 1), ' ');
        scr_put((byte)(x + 2), (byte)(y + 1), caps((byte)s[0]));
    }
    scr_put((byte)(x + 3), (byte)(y + 1), suit);
}

// ─── Upstream's calls ──────────────────────────────────────────────

void enableDoubleBuffer()
{
    drawDirect = 0;
}

void disableDoubleBuffer()
{
    scr_present();
    drawDirect = 1;
}

void drawBuffer()
{
    scr_present();
}

// The table, but for the status bar: the bar at the top (with the table
// joined, from the query), the rail, the felt.
void resetScreen()
{
    byte y, f = felt();
    const char *t;

    scr_ink(C_TEXT, C_BAR);
    scr_fill(0, ROW_BAR, WIDTH, 1, ' ');
    scr_ink(C_TEXT, C_BAR);
    scr_put(1, ROW_BAR, G_SPADE);
    scr_put(4, ROW_BAR, G_CLUB);
    scr_ink(C_RED, C_BAR);
    scr_put(2, ROW_BAR, G_HEART);
    scr_put(3, ROW_BAR, G_DIAMOND);
    drawCaps(6, ROW_BAR, "TEXAS HOLD'EM", C_GOLD, 255);
    for (t = query; *t && strncmp(t, "table=", 6); t++)
        ;
    if (*t) {
        t += 6;
        for (y = 0; t[y] && t[y] != '&'; y++)
            ;
        if (y > 8)
            y = 8;
        drawCaps((byte)(WIDTH - 2 - y), ROW_BAR, t, C_TEXT, y);
        drawCaps((byte)(WIDTH - 8 - y), ROW_BAR, "TABLE", C_DIM, 5);
    } else {
        drawCaps(WIDTH - 9, ROW_BAR, "FUJINET", C_DIM, 255);
    }

    scr_ink(C_TEXT, C_RAIL);
    scr_fill(0, ROW_RAIL_TOP, WIDTH, 1, ' ');
    scr_fill(0, ROW_RAIL_BOT, WIDTH, 1, ' ');
    for (y = ROW_RAIL_TOP + 1; y < ROW_RAIL_BOT; y++) {
        scr_ink(C_TEXT, C_RAIL);
        scr_put(0, y, ' ');
        scr_put(WIDTH - 1, y, ' ');
        scr_ink(C_TEXT, f);
        scr_fill(1, y, WIDTH - 2, 1, ' ');
    }
    show();
}

// The suits in the rail's corners, for the screens that are not the game
void drawBorder()
{
    scr_ink(C_GOLD, C_RAIL);
    scr_put(2, ROW_RAIL_TOP, G_SPADE);
    scr_put(WIDTH - 3, ROW_RAIL_TOP, G_HEART);
    scr_put(2, ROW_RAIL_BOT, G_DIAMOND);
    scr_put(WIDTH - 3, ROW_RAIL_BOT, G_CLUB);
    show();
}

void drawLogo()
{
    drawTitleLetters();
    show();
}

void clearStatusBar()
{
    byte x;
    scr_ink(C_TEXT, C_STATUS);
    scr_fill(0, ROW_STATUS, WIDTH, 2, ' ');
    for (x = 0; x < WIDTH; x++)
        keys_ch[x] = ' ';
    show();
}

// Capitals on the status bar, the figures in gold.  "R-EFRESH" is a key
// and what it does: the key goes in gold, and the hyphen is left out.
void drawStatusTextAt(unsigned char x, const char *s)
{
    const char *start = s;
    byte c, fg;
    for (; *s && x < WIDTH; s++) {
        c = caps((byte)*s);
        fg = c >= '0' && c <= '9' ? C_GOLD : C_TEXT;
        if (c >= 'A' && c <= 'Z' && s[1] == '-' && s[2] >= 'A' && s[2] <= 'Z'
            && (s == start || s[-1] == ' ')) {
            fg = C_GOLD;
            s++;
        }
        scr_ink(fg, C_STATUS);
        scr_put(x++, ROW_STATUS, c);
    }
    show();
}

void drawStatusText(const char *s)
{
    clearStatusBar();
    drawStatusTextAt(1, s);
}

// The keys for a move, in lower case where they say what the key does
// (G_LARROW, G_RARROW, G_UARROW, G_DARROW)
#define MOVE_KEYS "\x8F\x90 move  \x91\x92 amount  ENTER bet  ESC menu"

// The countdown's "s", and the keys on the row below the moves
void drawStatusTimer()
{
    const char *s = MOVE_KEYS;
    byte x = (byte)(WIDTH - 1 - strlen(s)), c;
    scr_ink(C_DIM, C_STATUS);
    scr_put(WIDTH - 2, ROW_STATUS, 's');
    for (; *s; s++, x++) {
        c = (byte)*s;
        keys_fg[x] = c >= 'a' && c <= 'z' ? C_DIM : C_GOLD;
        keys_ch[x] = caps(c);
        scr_ink(keys_fg[x], C_STATUS);
        scr_put(x, ROW_KEYS, keys_ch[x]);
    }
    show();
}

void drawTextAt(unsigned char x, unsigned char y, const char *s)
{
    drawCaps(x, y, s, C_TEXT, 255);
    show();
}

void drawText(unsigned char x, unsigned char y, const char *s)
{
    drawCaps(x, y, s, C_TEXT, 255);
    show();
}

void drawChip(unsigned char x, unsigned char y)
{
    scr_ink(C_GOLD, C_KEEP);
    scr_put(x, y, G_CHIP);
    show();
}

void drawBlank(unsigned char x, unsigned char y)
{
    scr_ink(C_TEXT, C_KEEP);
    scr_put(x, y, ' ');
    show();
}

// Under the moves, the underline is a gold bar; elsewhere, a gold line.
void drawLine(unsigned char x, unsigned char y, unsigned char w)
{
    if (y == ROW_KEYS)
        scr_ink(C_GOLD, C_STATUS);
    else
        scr_ink(C_GOLD, C_KEEP);
    for (; w && x < WIDTH; w--, x++)
        scr_put(x, y, y == ROW_KEYS ? G_UPPER : G_HLINE);
    show();
}

void hideLine(unsigned char x, unsigned char y, unsigned char w)
{
    for (; w && x < WIDTH; w--, x++) {
        if (y == ROW_KEYS) {
            scr_ink(keys_fg[x], C_STATUS);
            scr_put(x, y, keys_ch[x]);
        } else {
            scr_ink(C_TEXT, C_KEEP);
            scr_put(x, y, ' ');
        }
    }
    show();
}

// A box with round corners, w by h inside, filled dark
void drawBox(unsigned char x, unsigned char y, unsigned char w, unsigned char h)
{
    byte n, r = (byte)(x + w + 1), b = (byte)(y + h + 1);
    scr_ink(C_GOLD, C_PANEL);
    scr_put(x, y, G_ARC_UL);
    scr_put(r, y, G_ARC_UR);
    scr_put(x, b, G_ARC_LL);
    scr_put(r, b, G_ARC_LR);
    for (n = (byte)(x + 1); n < r; n++) {
        scr_put(n, y, G_HLINE);
        scr_put(n, b, G_HLINE);
    }
    for (n = (byte)(y + 1); n < b; n++) {
        scr_put(x, n, G_VLINE);
        scr_put(r, n, G_VLINE);
    }
    scr_fill((byte)(x + 1), (byte)(y + 1), w, h, ' ');
    show();
}

void initGraphics()
{
    scr_init(felt());
    clearStatusBar();
    scr_present();
}

void resetGraphics()
{
    scr_done();
}

void waitvsync()
{
    term_sleep(1);
}

uint8_t cycleNextColor()
{
    colorMode = (unsigned char)((colorMode + 1) % 3);
    return colorMode;
}

void setColorMode(int mode)
{
    colorMode = (unsigned char)(mode >= 0 && mode < 3 ? mode : 0);
}
