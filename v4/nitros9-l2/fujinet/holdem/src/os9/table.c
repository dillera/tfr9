/*
  The table on NitrOS-9, in 80 columns: upstream's gamelogic.c draws it
  for 32 and 40 (and so leaves these out for BUILD_OS9), with cards two
  columns wide.  Here a card is 5 by 3, with round corners, and each seat
  is a block 12 wide: the name plate, the two hole cards, and the chips
  below; the bet and the last move go beside it, toward the middle, where
  the five community cards lie, the street above them and the pot below.

        col  2      14            34            54          66
    row  2          [top left]    [top]         [top right]
         7          bet           bet           bet
         9   [left]        ---- FLOP ----           [right]
        10    bet     [] [] [] [] []    bet
        13                    POT 120
        15          bet           bet           bet
        16          [bottom left] [YOU]         [bottom right]

  upstream's showGameScreen() draws the whole table every time the state
  comes in: screen.c sends only what changed.
  @license gpl v. 3, see fujinet/LICENSE for details.
*/

#include "../misc.h"
#include "../gamelogic.h"
#include "../platform-specific/sound.h"
#include "table.h"

extern unsigned char redrawGameScreen;

#define SEAT_W 12

/*
  The eight seats, round from the bottom (where the player sits) by the
  left: the top left corner of each block; where its bet goes, from
  there; and which way the bet runs (0: a row under or over the block;
  1: two rows to its right; -1: two rows to its left, ending there).
*/
unsigned char playerXMaster[8] = { 34, 14,  2, 14, 34, 54, 66, 54 };
unsigned char playerYMaster[8] = { 16, 16,  9,  2,  2,  2,  9, 16 };
char playerDirMaster[8]        = {  0,  0,  1,  0,  0,  0, -1,  0 };
char playerBetXMaster[8]       = {  0,  0, 13,  0,  0,  0, -2,  0 };
char playerBetYMaster[8]       = { -1, -1,  1,  5,  5,  5,  1, -1 };

// The seats taken by 2 to 8 players, eight to a count
char playerCountIndex[56] = {
    0, 4, 0, 0, 0, 0, 0, 0,
    0, 3, 5, 0, 0, 0, 0, 0,
    0, 2, 4, 6, 0, 0, 0, 0,
    0, 2, 3, 5, 6, 0, 0, 0,
    0, 2, 3, 4, 5, 6, 0, 0,
    0, 1, 2, 3, 5, 6, 7, 0,
    0, 1, 2, 3, 4, 5, 6, 7
};

#define BOARD_X 25      // the community cards, 6 columns apart
#define BOARD_Y 10
#define STREET_Y 9
#define POT_Y 13

// Felt over part of a row
static void clearFelt(byte x, byte y, byte w)
{
    scr_ink(C_TEXT, felt());
    scr_fill(x, y, w, 1, ' ');
}

// The pot, under the board; and for a player alone at the table, who
// has no cards drawn, the board's empty places
void drawPot()
{
    byte w = numberWidth(state.pot), x;
    if (state.playerCount < 2)
        for (x = 0; x < 5; x++)
            drawCardFace((byte)(BOARD_X + x * (CARD_W + 1)), BOARD_Y, 0);
    clearFelt(BOARD_X, POT_Y, 29);
    if (!state.pot && state.round == 0)
        return;
    x = (byte)(40 - (6 + w) / 2);
    x = drawCaps(x, POT_Y, "POT ", C_DIM, 4);
    scr_ink(C_GOLD, C_KEEP);
    scr_put(x, POT_Y, G_CHIP);
    drawNumber((byte)(x + 2), POT_Y, state.pot, C_GOLD);
}

// The street, over the board, in a line across it
void drawStreetLabel()
{
    const char *s;
    byte n, x;
    if (state.round > 5 || state.playerCount < 2)
        return;
    switch (state.round) {
    case 1: s = "PRE-FLOP"; break;
    case 2: s = "FLOP"; break;
    case 3: s = "TURN"; break;
    case 4: s = "RIVER"; break;
    case 5: s = "SHOWDOWN"; break;
    default: s = ""; break;
    }
    n = (byte)strlen(s);
    scr_ink(felt_line(), felt());
    for (x = 0; x < 29; x++)
        scr_put((byte)(BOARD_X + x), STREET_Y, *s ? G_HLINE : ' ');
    if (*s) {
        x = (byte)(BOARD_X + 14 - (n + 2) / 2);
        scr_put(x, STREET_Y, ' ');
        x = drawCaps((byte)(x + 1), STREET_Y, s, C_GOLD, 255);
        scr_put(x, STREET_Y, ' ');
    }
}

// Whether the result names this player first ("BOB WON WITH ...")
static byte won(const char *name)
{
    const char *r = state.lastResult;
    byte a, b;
    if (state.round != 5 || !*name)
        return 0;
    for (; *name; name++, r++) {
        a = (byte)*name;
        b = (byte)*r;
        if ((a | 0x20) != (b | 0x20))
            return 0;
    }
    return *r == ' ';
}

/*
  Each seat's name plate, gold for the player whose move it is, and at the
  showdown for the winner; dim for a player who has folded or waits for
  the next hand; and the chips below.
*/
void drawNamePurse()
{
    byte n, sx, sy, plate, fg, you, w;
    Player *p;
    for (n = 0; n < state.playerCount; n++) {
        p = &state.players[n];
        sx = playerX[n];
        sy = playerY[n];
        you = n == 0 && !state.viewing;
        fg = C_TEXT;
        plate = you ? C_PLATE_YOU : C_PLATE;
        if (state.activePlayer == (signed char)n) {
            plate = C_GOLD;
            fg = C_BLACK;
        } else if (won(p->name)) {
            plate = C_GOLD;     // no one has the move at the showdown
            fg = C_BLACK;
        } else if (p->status == 2 || (p->status == 0 && state.round)) {
            fg = C_DIM;
        }
        scr_ink(fg, plate);
        scr_fill(sx, sy, SEAT_W, 1, ' ');
        if (state.activePlayer == (signed char)n)
            scr_put(sx, sy, G_POINT_R);
        drawCaps((byte)(sx + 1), sy, p->name, fg, 8);
        if (you)
            drawCaps((byte)(sx + SEAT_W - 3), sy, "YOU", plate == C_GOLD ? C_BLACK : C_GOLD, 3);

        clearFelt(sx, (byte)(sy + 4), SEAT_W);
        w = numberWidth(p->purse);
        scr_ink(C_GOLD, C_KEEP);
        scr_put((byte)(sx + 1), (byte)(sy + 4), G_CHIP);
        drawNumber((byte)(sx + 3), (byte)(sy + 4), p->purse, C_TEXT);
    }
}

// Where seat n's bet goes, and how wide it is
static byte slotX(byte n)
{
    return (byte)(playerX[n] + playerBetX[n]);
}

static byte slotY(byte n)
{
    return (byte)(playerY[n] + playerBetY[n]);
}

#define SIDE_W 8        // the slot beside a seat at the side

static void clearSlot(byte n)
{
    byte sx = slotX(n), sy = slotY(n);
    if (!playerDir[n]) {
        clearFelt(sx, sy, SEAT_W);
    } else {
        if (playerDir[n] < 0)
            sx = (byte)(sx - SIDE_W + 1);
        clearFelt(sx, sy, SIDE_W);
        clearFelt(sx, (byte)(sy + 1), SIDE_W);
    }
}

/*
  The bets and the last moves, while the betting goes on.  Under or over
  the seat: the move at the left, the bet at the right.  Beside it: the
  move, and the bet under it.
*/
void drawBets()
{
    byte n, sx, sy, w, mx, by;
    signed char d;
    Player *p;
    for (n = 0; n < state.playerCount; n++)
        clearSlot(n);
    if (state.round < 1 || state.round > 4)
        return;
    // p is stepped, not set to &state.players[n]: here CMOC 0.1.97
    // compiled that as n * 33, leaving out the players' address.
    p = state.players;
    for (n = 0; n < state.playerCount; n++, p++) {
        sx = slotX(n);
        sy = slotY(n);
        d = playerDir[n];
        w = (byte)strlen(p->move);
        if (w > 7)
            w = 7;
        mx = d < 0 ? (byte)(sx + 1 - w) : (d ? sx : (byte)(sx + 1));
        if (w)
            drawCaps(mx, sy, p->move, C_TEXT, 7);
        else if (p->status == 0)
            drawCaps(d < 0 ? (byte)(sx - 4) : mx, sy, "WAITS", C_DIM, 5);
        if (!p->bet)
            continue;
        w = (byte)(numberWidth(p->bet) + 2);
        by = d ? (byte)(sy + 1) : sy;
        if (d > 0)
            mx = sx;
        else if (d < 0)
            mx = (byte)(sx + 1 - w);
        else
            mx = (byte)(sx + SEAT_W - 1 - w);
        scr_ink(C_RED, C_KEEP);
        scr_put(mx, by, G_CHIP);
        drawNumber((byte)(mx + 2), by, p->bet, C_GOLD);
    }
}

// Seat n's hole card c (0 or 1), at its place
static void holeCard(byte n, byte c, const char *s)
{
    drawCardFace((byte)(playerX[n] + c * (CARD_W + 1)), (byte)(playerY[n] + 1), s);
}

// A seat's cards: none yet, two, or one back for a hand folded
static void seatCards(byte n)
{
    const char *hand = state.players[n].hand;
    byte len = (byte)strlen(hand);
    scr_ink(C_TEXT, felt());
    scr_fill(playerX[n], (byte)(playerY[n] + 1), SEAT_W, CARD_H, ' ');
    if (len >= 4) {
        holeCard(n, 0, hand);
        holeCard(n, 1, hand + 2);
    } else if (len >= 2) {
        drawCardFace((byte)(playerX[n] + 3), (byte)(playerY[n] + 1), hand);
    }
}

// Shows a card just dealt, with its sound
static void dealt(byte ticks)
{
    scr_present();
    soundDealCard();
    pause(ticks);
}

/*
  The hole cards and the board.  As upstream's: the hole cards are dealt
  round the table, a card at a time, when they first come; each street's
  new cards are turned one by one; and at the showdown the hands still in
  are turned over in turn.
*/
void drawCards(bool finalFlip)
{
    static byte cc, c, n, h, len, flip;
    const char *hand;

    for (c = 0; c < 5; c++)
        drawCardFace((byte)(BOARD_X + c * (CARD_W + 1)), BOARD_Y, 0);
    for (n = 0; n < state.playerCount; n++)
        if (state.round < 1 || !finalFlip)
            seatCards(n);
    if (state.round < 1)
        return;

    doAnim = !noAnim && !cardIndex && !finalFlip;
    if (doAnim) {
        // the seats empty, then the cards go round
        for (n = 0; n < state.playerCount; n++) {
            scr_ink(C_TEXT, felt());
            scr_fill(playerX[n], (byte)(playerY[n] + 1), SEAT_W, CARD_H, ' ');
        }
        scr_present();
        for (c = 0; c < 2; c++)
            for (h = 1; h <= state.playerCount; h++) {
                n = (byte)(h % state.playerCount);
                hand = state.players[n].hand;
                len = (byte)strlen(hand);
                if (len >= 4)
                    holeCard(n, c, hand + c * 2);
                else if (len >= 2 && !c)
                    drawCardFace((byte)(playerX[n] + 3), (byte)(playerY[n] + 1), hand);
                else
                    continue;
                dealt(5);
            }
    }
    cardIndex = 1;

    cc = (byte)(strlen(state.community) >> 1);
    for (c = 0; c < cc; c++) {
        drawCardFace((byte)(BOARD_X + c * (CARD_W + 1)), BOARD_Y, state.community + c * 2);
        if (!noAnim && c >= currentCard)
            dealt(10);
    }
    currentCard = cc;

    if (finalFlip) {
        // The showdown: the hands that are still in turn over one by one
        scr_present();
        for (h = 1; h < state.playerCount; h++) {
            hand = state.players[h].hand;
            flip = state.players[h].status == 1 && hand[0] != '?';
            if (!flip)
                continue;
            seatCards(h);
            dealt(35);
        }
        for (n = 0; n < state.playerCount; n++)
            seatCards(n);
    }
    noAnim = false;
}

// The gold plate shows whose move it is: nothing more to draw.
void highlightActivePlayer()
{
}

/*
  At the end of a street, the moves go, then the bets go into the pot one
  by one, as upstream's do.  The screen still shows the last state here.
*/
void animateChipsToPotOnRoundEnd()
{
    byte n;
    if (state.round <= prevRound || state.pot == 0)
        return;
    if (state.round < 5)
        redrawGameScreen = 1;
    clearStatusBar();
    scr_present();
    pause(50);
    for (n = 0; n < state.playerCount; n++)
        if (playerDir[n])
            clearFelt(playerDir[n] < 0 ? (byte)(slotX(n) - SIDE_W + 1) : slotX(n), slotY(n), SIDE_W);
        else
            clearFelt(slotX(n), slotY(n), 7);
    scr_present();
    pause(30);
    if (prevPot == state.pot)
        return;
    prevPot = state.pot;
    for (n = 0; n < state.playerCount; n++) {
        clearSlot(n);
        scr_present();
        soundTakeChip(n);
    }
    drawPot();
    scr_present();
    pause(45);
}
