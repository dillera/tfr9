/*
  Graphics functionality: NitrOS-9 on the TFR/911H, drawn on the tether's
  80 by 24 ANSI terminal through term.c, in color.

  A cell of a gamefield is two characters wide and one high, about square.
  The layout depends on the number of boards:
    1  placing ships: your board under the logo, your fleet beside it
    2  side by side, with coordinates, and whose turn it is between them
    3-4  a quadrant each, clockwise from the bottom left (you), as upstream
  @license gpl v. 3, see fujinet/LICENSE for details.
*/

#include "../misc.h"

#define SEA      TC_BLUE                // the water, a background
#define WAVE     TC_CYAN
#define FRAME    TC_CYAN
#define LABEL    TC_WHITE
#define TEXT     (TC_BRIGHT + TC_WHITE)
#define ALT      TC_CYAN
#define HILITE   (TC_BRIGHT + TC_YELLOW)
#define HULL     TC_WHITE
#define WHITE    (TC_BRIGHT + TC_WHITE)
#define YELLOW   (TC_BRIGHT + TC_YELLOW)
#define RED      (TC_BRIGHT + TC_RED)

// The graphics characters of glyphs.h, as UTF-8 four bytes apart.
const char glyph_table[] =
    "\xE2\x94\x80" "\0" "\xE2\x94\x82" "\0" "\xE2\x94\x8C" "\0" "\xE2\x94\x90" "\0"
    "\xE2\x94\x94" "\0" "\xE2\x94\x98" "\0" "\xE2\x94\xA4" "\0" "\xE2\x94\x9C" "\0"
    "\xE2\x96\x8C" "\0" "\xE2\x96\x90" "\0" "\xE2\x96\x84" "\0" "\xE2\x96\x80" "\0"
    "\xE2\x96\x88" "\0" "\xE2\x96\x91" "\0" "\xE2\x96\x92" "\0" "\xE2\x96\x93" "\0"
    "\xE2\x97\x84" "\0" "\xE2\x96\xBA" "\0" "\xE2\x96\xA0" "\0" "\xE2\x80\xA2" "\0"
    "\xE2\x97\x8B" "\0" "\xE2\x98\xBC" "\0" "\xE2\x89\x88" "\0" "\xC3\x97" "\0\0"
    "\xC2\xB7" "\0\0" "\xE2\x95\x90" "\0" "\xE2\x95\x91" "\0" "\xE2\x95\x94" "\0"
    "\xE2\x95\x97" "\0" "\xE2\x95\x9A" "\0" "\xE2\x95\x9D" "\0";

static byte boards;             // how many boards drawBoard() drew
static byte gx[4], gy[4];       // the top left cell of each, on the screen
static int8_t turn = -1;        // whose turn the screen shows, or -1

// The legend of a 2-player board: where each ship starts, from its left.
static const byte legend_x[5] = {0, 6, 11, 15, 19};

extern void title(void);
extern void drawLogo(void);
extern void stopSound(void);

static void ink(byte fg, byte bg)
{
    term_color(fg, bg);
}

static void repeat(byte ch, byte n)
{
    while (n--)
        term_putc((char)ch);
}

// Text from x on row y, cut at the right edge.  alt: in the alternate
// color, but its capital letters bright, as the CoCo's are.
static void text(byte x, byte y, const char *s, byte alt)
{
    if (y >= HEIGHT)
        y = HEIGHT - 1;
    term_gotoxy(x, y);
    for (; *s && x < WIDTH; s++, x++) {
        if (alt)
            ink(*s >= 'A' && *s <= 'Z' ? HILITE : ALT, TC_DEFAULT);
        else
            ink(TEXT, TC_DEFAULT);
        term_putc(*s);
    }
}

static void center(byte y, const char *s, byte alt)
{
    byte n = (byte)strlen(s);
    text(n < WIDTH ? (byte)((WIDTH - n) / 2) : 0, y, s, alt);
}

static void spaces(byte x, byte y, byte w)
{
    if (y >= HEIGHT)
        y = HEIGHT - 1;
    if (x >= WIDTH)
        return;
    if (w > WIDTH - x)
        w = WIDTH - x;
    term_gotoxy(x, y);
    ink(TC_DEFAULT, TC_DEFAULT);
    repeat(' ', w);
}

// ── Cells ──────────────────────────────────────────────────────────

static void at_cell(byte q, byte x, byte y)
{
    term_gotoxy(gx[q] + x * 2, gy[q] + y);
}

// Water, with a wave here and there, as upstream's CoCo 3 tiles have.
static void water(byte q, byte x, byte y)
{
    byte w = (byte)((q + y % 5 + x) % 6);
    ink(WAVE, SEA);
    term_putc(w == 0 ? '~' : ' ');
    term_putc(w == 3 ? '~' : ' ');
}

static void fire(byte frame)
{
    ink(YELLOW, TC_RED);
    term_putc((char)(frame ? G_MEDIUM : G_DARK));
    term_putc((char)(frame ? G_DARK : G_MEDIUM));
}

static void splash(void)
{
    ink(WHITE, SEA);
    term_putc((char)G_RING);
    term_putc(' ');
}

// A cell as the server has it: water, hit, or miss.
static void cell(byte q, byte x, byte y, byte v)
{
    at_cell(q, x, y);
    if (v == FIELD_ATTACK)
        fire(0);
    else if (v == FIELD_MISS)
        splash();
    else
        water(q, x, y);
}

// ── Ships ──────────────────────────────────────────────────────────

// part 0 is the bow, 1 amidships, 2 the stern.  The ends are narrower,
// but a whole square long, so that a ship looks as long as it is.
static void hull(byte vertical, byte part)
{
    ink(HULL, SEA);
    if (vertical) {
        term_putc((char)(part == 1 ? G_FULL : G_RIGHT));
        term_putc((char)(part == 1 ? G_FULL : G_LEFT));
    } else {
        term_putc((char)(part == 0 ? G_RIGHT : G_FULL));
        term_putc((char)(part == 2 ? G_LEFT : G_FULL));
    }
}

void drawShip(uint8_t quadrant, uint8_t size, uint8_t pos, bool hide)
{
    byte x, y, i, vertical = 0;

    if (pos > 99) {
        vertical = 1;
        pos -= 100;
    }
    x = pos % 10;
    y = pos / 10;
    for (i = 0; i < size && x < 10 && y < 10; i++) {
        at_cell(quadrant, x, y);
        if (hide)
            water(quadrant, x, y);
        else
            hull(vertical, i == 0 ? 0 : i == size - 1 ? 2 : 1);
        if (vertical)
            y++;
        else
            x++;
    }
}

// The fleet beside or under a board, a character to each part of a ship.
void drawLegendShip(uint8_t player, uint8_t index, uint8_t size, uint8_t status)
{
    byte x, y, i;

    if (boards > 2) {
        x = player > 1 ? 70 : 4;
        y = gy[player] + 1 + index * 2;
    } else {
        x = gx[player] + legend_x[index];
        y = gy[player] + 12;
    }
    term_gotoxy(x, y);
    if (!status) {
        ink(RED, TC_DEFAULT);
        repeat(G_TIMES, size);
        return;
    }
    ink(HULL, TC_DEFAULT);
    term_putc((char)G_LARROW);
    for (i = 2; i < size; i++)
        term_putc((char)G_SQUARE);
    term_putc((char)G_RARROW);
}

// ── Gamefields ─────────────────────────────────────────────────────

void drawGamefield(uint8_t quadrant, uint8_t *field)
{
    byte x, y;

    for (y = 0; y < 10; y++)
        for (x = 0; x < 10; x++, field++)
            if (*field)
                cell(quadrant, x, y, *field);
}

// anim 0/1: the two frames of a fire; 10-15: a shell coming down.
void drawGamefieldUpdate(uint8_t quadrant, uint8_t *gamefield, uint8_t attackPos, uint8_t anim)
{
    static const byte shell[6] = {G_RING, G_BULLET, G_DOT, G_SUN, G_LIGHT, G_DOT};
    byte c = gamefield[attackPos];

    at_cell(quadrant, attackPos % 10, attackPos / 10);
    if (anim > 9) {
        anim -= 10;
        if (anim == 3)
            ink(YELLOW, TC_RED);
        else
            ink(anim > 3 ? YELLOW : WHITE, SEA);
        repeat(shell[anim], 2);
    } else if (c == FIELD_ATTACK) {
        fire(anim);
    } else if (c == FIELD_MISS) {
        splash();
    }
}

// Where the cursor is, as a coordinate, between the boards.
static void target(byte x, byte y)
{
    char s[12];

    strcpy(s, "target ");
    s[7] = (char)('A' + x);
    if (y == 9) {
        s[8] = '1';
        s[9] = '0';
        s[10] = 0;
    } else {
        s[8] = (char)('1' + y);
        s[9] = ' ';
        s[10] = 0;
    }
    if (boards > 2) {
        text(37, 11, "target", 1);
        text(38, 12, s + 7, 0);
    } else {
        text(34, 12, s, 1);
    }
}

// blink 0: the cell as it is; 1, 2: the two frames of the cursor.
void drawGamefieldCursor(uint8_t quadrant, uint8_t x, uint8_t y, uint8_t *gamefield, uint8_t blink)
{
    if (!blink) {
        cell(quadrant, x, y, gamefield[y * 10 + x]);
        return;
    }
    at_cell(quadrant, x, y);
    if (blink == 1)
        ink(TC_BLACK, TC_YELLOW);
    else
        ink(YELLOW, SEA);
    term_putc('[');
    term_putc(']');
    target(x, y);
}

// ── Boards ─────────────────────────────────────────────────────────

static void layout(byte n)
{
    boards = n;
    if (n <= 1) {
        gx[0] = 30;
        gy[0] = 12;
    } else if (n == 2) {
        gx[0] = 8;
        gy[0] = 5;
        gx[1] = 52;
        gy[1] = 5;
    } else {
        gx[0] = gx[1] = 14;
        gx[2] = gx[3] = 46;
        gy[1] = gy[2] = 1;
        gy[0] = gy[3] = 13;
    }
}

// The frame of a board, and its water.
static void frame(byte q)
{
    byte x = gx[q] - 1, y = gy[q] - 1, r, c;

    ink(FRAME, TC_DEFAULT);
    term_gotoxy(x, y);
    term_putc((char)G_UL);
    repeat(G_HLINE, 20);
    term_putc((char)G_UR);
    for (r = 0; r < 10; r++) {
        term_gotoxy(x, gy[q] + r);
        ink(FRAME, TC_DEFAULT);
        term_putc((char)G_VLINE);
        for (c = 0; c < 10; c++)
            water(q, c, r);
        ink(FRAME, TC_DEFAULT);
        term_putc((char)G_VLINE);
    }
    term_gotoxy(x, y + 11);
    term_putc((char)G_LL);
    repeat(G_HLINE, 20);
    term_putc((char)G_LR);
}

// A to J over a board and 1 to 10 at its left.
static void coordinates(byte q)
{
    byte i;

    ink(LABEL, TC_DEFAULT);
    term_gotoxy(gx[q], gy[q] - 2);
    for (i = 0; i < 10; i++) {
        term_putc((char)('A' + i));
        term_putc(' ');
    }
    for (i = 0; i < 10; i++) {
        term_gotoxy(gx[q] - 4, gy[q] + i);
        if (i < 9)
            term_putc(' ');
        else
            term_putc('1');
        term_putc((char)(i < 9 ? '1' + i : '0'));
    }
}

static void keys(const char *s)
{
    center(HEIGHT - 1, s, 1);
}

void drawBoard(uint8_t playerCount)
{
    static const char *const fleet[5] = {
        "carrier", "battleship", "cruiser", "submarine", "destroyer"
    };
    byte i;

    layout(playerCount);
    turn = -1;
    for (i = 0; i < playerCount; i++) {
        frame(i);
        if (playerCount <= 2)
            coordinates(i);
    }
    if (playerCount <= 1) {
        drawLogo();
        text(58, gy[0], "your fleet", 1);
        for (i = 0; i < 5; i++) {
            text(58, gy[0] + 2 + i * 2, fleet[i], 1);
            ink(HULL, TC_DEFAULT);
            term_gotoxy(70, gy[0] + 2 + i * 2);
            term_putc((char)G_LARROW);
            repeat(G_SQUARE, shipSize[i] - 2);
            term_putc((char)G_RARROW);
        }
        keys("ARROWS move   R rotate   SPACE place   ESC menu");
    } else if (playerCount == 2) {
        keys("ARROWS or A-J 1-0 aim   SPACE fire   ESC menu");
    } else {
        text(37, 7, "ARROWS", 1);
        text(37, 8, "A-J 1-0", 1);
        text(37, 9, "aim", 1);
        text(37, 14, "SPACE", 1);
        text(37, 15, "fire", 1);
    }
}

// Placing ships: the one being placed, in the fleet beside the board, and
// those already placed.
void drawFleetMark(uint8_t index)
{
    byte i;

    if (boards > 1)
        return;
    for (i = 0; i < 5; i++) {
        term_gotoxy(56, gy[0] + 2 + i * 2);
        if (i < index) {
            ink(TC_BRIGHT + TC_GREEN, TC_DEFAULT);
            term_putc((char)G_BULLET);
        } else if (i == index) {
            ink(YELLOW, TC_DEFAULT);
            term_putc((char)G_RARROW);
        } else {
            ink(TC_DEFAULT, TC_DEFAULT);
            term_putc(' ');
        }
    }
}

// No one's turn, between the boards.
static void clear_turn(void)
{
    byte y;

    if (turn < 0)
        return;
    if (boards == 2) {
        spaces(30, 8, 17);
        spaces(30, 9, 17);
    } else if (boards > 2) {
        y = gy[turn] + 4;
        spaces(35, y, 10);
        spaces(35, y + 1, 10);
    }
    turn = -1;
}

// The name, in the frame: over a board, or under a bottom quadrant.
void drawPlayerName(uint8_t player, const char *name, bool active)
{
    byte x = gx[player] + 1, y, n = (byte)strlen(name), i;

    if (player >= boards)
        return;
    if (n > 8)
        n = 8;
    y = boards > 2 && (player == 0 || player == 3) ? gy[player] + 10 : gy[player] - 1;

    term_gotoxy(x, y);
    ink(FRAME, TC_DEFAULT);
    term_putc((char)G_RTEE);
    if (active)
        ink(TC_BLACK, TC_YELLOW);
    else
        ink(WHITE, TC_DEFAULT);
    term_putc(' ');
    term_putc((char)(active ? G_RARROW : ' '));
    term_putc(' ');
    for (i = 0; i < 8; i++)
        term_putc(i < n ? name[i] : ' ');
    ink(FRAME, TC_DEFAULT);
    term_putc((char)G_LTEE);

    // Whose turn it is, between the boards
    if (!active && turn != (int8_t)player)
        return;
    clear_turn();
    if (!active)
        return;
    turn = (int8_t)player;
    if (boards == 2) {
        if (player == 0) {
            text(34, 8, "YOUR TURN", 1);
            term_gotoxy(31, 9);
            ink(YELLOW, TC_DEFAULT);
            repeat(G_HLINE, 14);
            term_putc((char)G_RARROW);
        } else {
            term_gotoxy(38 - (n + 7) / 2, 8);
            ink(RED, TC_DEFAULT);
            for (i = 0; i < n; i++)
                term_putc(name[i]);
            term_puts(" fires");
            term_gotoxy(31, 9);
            term_putc((char)G_LARROW);
            repeat(G_HLINE, 14);
        }
    } else if (boards > 2) {
        y = gy[player] + 4;
        ink(player ? RED : YELLOW, TC_DEFAULT);
        if (player < 2) {
            term_gotoxy(35, y);
            term_putc((char)G_LARROW);
            term_putc(' ');
        } else {
            term_gotoxy(43 - n, y);     // "name ►", ending at column 44
        }
        for (i = 0; i < n; i++)
            term_putc(name[i]);
        if (player >= 2) {
            term_putc(' ');
            term_putc((char)G_RARROW);
        }
        term_gotoxy(player < 2 ? 37 : 40, y + 1);
        term_puts(player ? "fires" : "fire!");
    }
}

// The end of a game, in a box across the middle.
void drawEndgameMessage(const char *message)
{
    static const char press[] = "press SPACE";
    byte n = (byte)strlen(message), w, x, y, i;

    clear_turn();
    w = (n > sizeof press - 1 ? n : sizeof press - 1) + 4;
    x = (byte)((WIDTH - w - 2) / 2);
    y = boards > 2 ? 10 : 18;
    ink(YELLOW, TC_BLUE);
    term_gotoxy(x, y);
    term_putc((char)G_DUL);
    repeat(G_DH, w);
    term_putc((char)G_DUR);
    for (i = 1; i <= 2; i++) {
        term_gotoxy(x, y + i);
        ink(YELLOW, TC_BLUE);
        term_putc((char)G_DV);
        repeat(' ', w);
        term_putc((char)G_DV);
    }
    term_gotoxy(x, y + 3);
    term_putc((char)G_DLL);
    repeat(G_DH, w);
    term_putc((char)G_DLR);
    term_gotoxy(x + 1 + (w - n) / 2, y + 1);
    ink(WHITE, TC_BLUE);
    term_puts(message);
    term_gotoxy(x + 1 + (w - (sizeof press - 1)) / 2, y + 2);
    ink(TC_CYAN, TC_BLUE);
    term_puts(press);
}

// The timer's clock, after the seconds at the bottom right.
void drawClock()
{
    term_gotoxy(WIDTH - 1, HEIGHT - 1);
    ink(YELLOW, TC_DEFAULT);
    term_putc((char)G_RING);
}

// The server not answering, at the bottom left.
void drawConnectionIcon(bool show)
{
    if (!show) {
        spaces(0, HEIGHT - 1, 7);
        return;
    }
    term_gotoxy(0, HEIGHT - 1);
    ink(RED, TC_DEFAULT);
    term_puts("offline");
}

// ── Text and the rest ──────────────────────────────────────────────

void drawText(uint8_t x, uint8_t y, const char *s)
{
    text(x, y, s, 0);
}

void drawTextAlt(uint8_t x, uint8_t y, const char *s)
{
    text(x, y, s, 1);
}

void drawIcon(uint8_t x, uint8_t y, uint8_t icon)
{
    term_gotoxy(x, y < HEIGHT ? y : HEIGHT - 1);
    switch (icon) {
    case ICON_TEXT_CURSOR:
        ink(YELLOW, TC_DEFAULT);
        term_putc((char)G_LEFT);
        break;
    case ICON_PLAYER:
        ink(TC_BRIGHT + TC_GREEN, TC_DEFAULT);
        term_putc((char)G_BULLET);
        break;
    case ICON_MARK_ALT:
        ink(TC_YELLOW, TC_DEFAULT);
        term_putc((char)G_RARROW);
        break;
    default:
        ink(YELLOW, TC_DEFAULT);
        term_putc((char)G_RARROW);
        break;
    }
}

void drawBlank(uint8_t x, uint8_t y)
{
    spaces(x, y, 1);
}

void drawSpace(uint8_t x, uint8_t y, uint8_t w)
{
    spaces(x, y, w);
}

void drawLine(uint8_t x, uint8_t y, uint8_t w)
{
    term_gotoxy(x, y);
    ink(FRAME, TC_DEFAULT);
    repeat(G_HLINE, w);
}

// A frame around w by h characters, whose top left corner is at x, y.
void drawBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    byte r;

    ink(FRAME, TC_DEFAULT);
    term_gotoxy(x, y);
    term_putc((char)G_DUL);
    repeat(G_DH, w);
    term_putc((char)G_DUR);
    for (r = 1; r <= h; r++) {
        term_gotoxy(x, y + r);
        ink(FRAME, TC_DEFAULT);
        term_putc((char)G_DV);
        term_gotoxy(x + w + 1, y + r);
        term_putc((char)G_DV);
    }
    term_gotoxy(x, y + h + 1);
    term_putc((char)G_DLL);
    repeat(G_DH, w);
    term_putc((char)G_DLR);
}

void resetScreen()
{
    ink(TC_DEFAULT, TC_DEFAULT);
    term_clrscr();
    turn = -1;
}

uint8_t cycleNextColor()
{
    return 0;
}

bool saveScreenBuffer()
{
    return false;
}

void restoreScreenBuffer()
{
}

void initGraphics()
{
    term_init();
    term_glyphs(glyph_table, N_GLYPHS);
    title();
}

void resetGraphics()
{
    stopSound();
    term_glyphs(0, 0);
    term_done();
}

void waitvsync()
{
    term_sleep(1);
}
