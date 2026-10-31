// table.h -- Texas Hold'em on NitrOS-9: the look of the table, shared by
// graphics.c (upstream's drawing calls), table.c (the seats, cards and
// pot) and title.c (the title screen).
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#ifndef TABLE_H
#define TABLE_H

#include "screen.h"

// The rows of the screen
#define ROW_BAR      0      // the top bar: the game's name, the table
#define ROW_RAIL_TOP 1      // the table's rail, round the felt
#define ROW_RAIL_BOT 21
#define ROW_STATUS   22     // the status bar: messages and the moves,
#define ROW_KEYS     23     // with the move's underline and the keys below

// Colours, from xterm's 256.  The felt is green, blue or red (C toggles
// it): felt() and felt_line() (a lighter shade, for the lines on it).
#define C_BAR        234
#define C_STATUS     235
#define C_RAIL       94
#define C_TEXT       230
#define C_DIM        245
#define C_GOLD       220
#define C_RED        160
#define C_BLACK      16
#define C_WHITE      231
#define C_BACK       25     // the pattern on a card's back
#define C_PLATE      236    // a name plate
#define C_PLATE_YOU  18
#define C_PANEL      234    // inside a box

byte felt(void);
byte felt_line(void);

extern byte drawDirect;     // disableDoubleBuffer(): each drawing is shown at once
void show(void);            // shows what was drawn, if drawDirect

// A card, 5 by 3, its top left at x, y: s is its rank and suit ("as",
// "td"), "??" for its back, or a null pointer for an empty place.
void drawCardFace(byte x, byte y, const char *s);
#define CARD_W 5
#define CARD_H 3

// Text in capitals, in fg over what is there; at most max characters.
// Returns the column after it.
byte drawCaps(byte x, byte y, const char *s, byte fg, byte max);
// A number, and how many characters it takes
byte drawNumber(byte x, byte y, word n, byte fg);
byte numberWidth(word n);

void drawTitleLetters(void);    // title.c: "TEXAS HOLD'EM", big

#endif
