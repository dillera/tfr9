// screen.h -- Texas Hold'em's drawing on the tether's terminal: ANSI, in
// xterm's 256 colours, through fujinet/c/term.c.  Everything is drawn into
// a copy of the screen, and scr_present() sends the terminal only the
// cells that differ from what it shows, so a table drawn whole for every
// poll of the server costs only what moved.
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#ifndef SCREEN_H
#define SCREEN_H

#include "term.h"
#include "glyphs.h"

#define SCR_COLS 80
#define SCR_ROWS 24

// A background of C_KEEP keeps the one already drawn in the cell, so that
// text can go over a box or the felt alike.  (xterm's colour 0 is never
// used: black is 16.)
#define C_KEEP 0

void scr_init(byte bg);         // raw keys, cursor off, the screen cleared to bg
void scr_done(void);            // the terminal as it was
void scr_clear(byte bg);        // clears the terminal itself to bg, and the copy
void scr_ink(byte fg, byte bg); // the colours of what is drawn next
void scr_put(byte x, byte y, byte ch);  // ASCII, or a G_ code from glyphs.h
byte scr_text(byte x, byte y, const char *s);  // returns the column after it
void scr_fill(byte x, byte y, byte w, byte h, byte ch);
byte scr_bg_at(byte x, byte y);
void scr_present(void);         // sends what changed

#endif
