// term.h -- the console of the FujiNet C programs: the tether's terminal
// (ANSI, at least 80 by 24), keys read raw, and a copy of the screen, so
// that a row can be shown inverted and back again, as the CoCo programs
// these come from do by flipping the attributes in screen memory.

#ifndef TERM_H
#define TERM_H

#include "fntypes.h"

#define TERM_COLS 80
#define TERM_ROWS 24

// The keys, as term_getc() returns them.  The arrows are also ^P ^N ^B ^F.
#define KEY_UP     0x10
#define KEY_DOWN   0x0E
#define KEY_LEFT   0x02
#define KEY_RIGHT  0x06
#define KEY_PGUP   0x15     // ^U
#define KEY_PGDN   0x04     // ^D
#define KEY_BS     0x08     // Backspace or Delete
#define KEY_ENTER  0x0D
#define KEY_BREAK  0x03     // Escape or ^C (or ^E)

void term_init(void);       // raw keys, cursor off, screen cleared
void term_done(void);       // the terminal as it was, cursor on

void term_putc(char c);     // '\n' is a new line
void term_puts(const char *s);
int term_printf(const char *fmt, ...);
void term_flush(void);

void term_gotoxy(byte x, byte y);
byte term_wherex(void);
byte term_wherey(void);
void term_clrscr(void);
void term_clreol(void);     // to the end of the row, at the cursor
void term_reverse(byte on); // what follows is in reverse video
void term_color(byte fg, byte bg);  // what follows is in these colors

// Colors for term_color(); 0 is the terminal's own.  TC_BRIGHT plus a
// color (red to white) is its bright version, for the foreground.
#define TC_DEFAULT 0
#define TC_BLACK   1
#define TC_RED     2
#define TC_GREEN   3
#define TC_YELLOW  4
#define TC_BLUE    5
#define TC_MAGENTA 6
#define TC_CYAN    7
#define TC_WHITE   8
#define TC_BRIGHT  7
void term_invert_row(byte y);   // flips row y between normal and reverse
void term_frame(byte on);   // on: draw unseen; off: send only what changed

// Graphics characters, which term_putc() sends as UTF-8.
#define TG_HLINE     0x80   // box drawing
#define TG_VLINE     0x81
#define TG_ULCORNER  0x82
#define TG_URCORNER  0x83
#define TG_LLCORNER  0x84
#define TG_LRCORNER  0x85
#define TG_RTEE      0x86
#define TG_LTEE      0x87
#define TG_TTEE      0x88
#define TG_BTEE      0x89
#define TG_CROSS     0x8A
#define TG_LEFTHALF  0x8B   // blocks
#define TG_RIGHTHALF 0x8C
#define TG_LOWERHALF 0x8D
#define TG_UPPERHALF 0x8E
#define TG_QUAD_LL   0x8F   // quadrants: one, three, two
#define TG_QUAD_LR   0x90
#define TG_QUAD_UL   0x91
#define TG_QUAD_UR   0x92
#define TG_QUAD_NOLL 0x93
#define TG_QUAD_NOLR 0x94
#define TG_QUAD_NOUL 0x95
#define TG_QUAD_NOUR 0x96
#define TG_FULL      0x97
#define TG_QUAD_ULLR 0x98
#define TG_QUAD_URLL 0x99
void term_cursor(byte on);
void term_bell(void);
void term_raw(const char *s);   // sent as it is, not kept in the copy

byte term_ready(void);      // a key is waiting
byte term_getc(void);       // waits for a key
byte term_inkey(void);      // a key, or 0 if none is waiting
void term_getline(char *buf, byte size);    // edits a line; BREAK leaves it empty

void term_sleep(word ticks);    // sixtieths of a second
dword term_seconds(void);   // seconds since midnight

#endif // TERM_H
