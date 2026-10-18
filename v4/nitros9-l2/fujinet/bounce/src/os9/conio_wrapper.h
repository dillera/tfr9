#ifndef OS9_CONIO_WRAPPER_H
#define OS9_CONIO_WRAPPER_H

// NitrOS-9: the conio calls of the client on the tether's terminal,
// through fujinet/c/term.c.
//
// The world is 40 or 80 places wide (screen.h).  At 40 a place is two
// columns of the terminal, so gotoxy() doubles x; text is written a column
// to a character from there, and the shapes a place to a character, by
// cputc_cell().  TEXT_X() is the x for gotoxy() of a column.

#include <cmoc.h>
#include "term.h"
#include "chardef.h"
#include "stdbool.h"

#undef gotoxy
#undef gotox
#undef gotoy
#undef wherex
#undef wherey
#undef revers
#undef cursor
#undef cputc
#undef cputs
#undef chlinexy
#undef cputsxy
#undef kbhit
#undef cgetc
#undef isprint

extern byte cell_shift;     // 1 if a place is two columns, 0 if one
void set_screen_width(byte width);  // 40 or 80

#define gotoxy(x, y)    term_gotoxy((byte)((x) << cell_shift), (byte)(y))
#define gotox(x)        gotoxy((x), term_wherey())
#define gotoy(y)        term_gotoxy(term_wherex(), (byte)(y))
#define wherex()        (term_wherex() >> cell_shift)
#define TEXT_X(col)     ((col) >> cell_shift)
#define wherey()        term_wherey()

#define clrscr()        term_clrscr()
#define cputc(c)        term_putc((char)(c))
#define cputs(s)        term_puts(s)
#define revers(on)      term_reverse((byte)(on))
#define cursor(on)      term_cursor((byte)(on))
#define isprint(c)      ((c) >= 0x20 && (c) <= 0x7E)

// The key waiting, or 0 (and a tick's sleep) if there is none.
#define kbhit()         os9_key()
char os9_key(void);
byte cgetc(void);

void cputcxy(unsigned char x, unsigned char y, char c);
void cputsxy(unsigned char x, unsigned char y, const char *s);
void chlinexy(unsigned char x, unsigned char y, unsigned char length);

// A place of shape number `shape`, in the shape's color.
void cputc_cell(char c, uint8_t shape);

unsigned char doesclrscrafterexit(void);

// main() calls this on a CMOC build: the CoCo's starts its hi-res text
// screen, this the terminal.
void hirestxt_init(void);

void switch_colorset(void);

#endif // OS9_CONIO_WRAPPER_H
