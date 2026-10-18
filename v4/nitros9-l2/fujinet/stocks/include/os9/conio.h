#ifndef CONIO_H
#define CONIO_H

/* -----------------------------------------------------------------------
 * conio.h  –  NitrOS-9: the conio calls on the tether's terminal, through
 * term.c (../../../c/term.h).
 * ----------------------------------------------------------------------- */

#include "term.h"
#include <chardefs.h>

#ifndef _CMOC_VERSION_
/* Testing on Unix: printf() must go through term.c's copy of the screen. */
#include <stdio.h>
#undef putchar
#define printf term_printf
#define putchar term_putc
#endif

#define clrscr()          term_clrscr()
#define gotoxy(x, y)      term_gotoxy((byte)(x), (byte)(y))
#define wherex()          term_wherex()
#define wherey()          term_wherey()
#define revers(on)        term_reverse((byte)(on))
#define cursor(on)        term_cursor((byte)(on))
#define cputs(s)          term_puts(s)
#define cputc(c)          term_putc(c)
#define cputcxy(x, y, c)  do { gotoxy((x), (y)); term_putc(c); } while (0)

bool kbhit(void);
char cgetc(void);

#endif /* CONIO_H */
