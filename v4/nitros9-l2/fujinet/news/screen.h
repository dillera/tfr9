/**
 * @brief   News Reader
 * @license gpl v. 3, see LICENSE for details.
 * @verbose The screen: the CoCo's text screen routines (cocotext.c, bar.c),
 *          on the tether's terminal through ../c/term.c
 */

#ifndef SCREEN_H
#define SCREEN_H

#include "term.h"

#define printf term_printf
#define putchar term_putc
#define gotoxy(x, y) term_gotoxy((byte)(x), (byte)(y))

void clear_screen(byte color);
void bar(byte y);
void print_reverse(byte x, byte y, const char *text, bool lowercase_only);
char *screen_upper(const char *s);
char *screen_lower(const char *s);
void show_fetching_msg(bool clear, bool articles);
void get_line(char *buf, uint8_t max_len);
byte waitkey(byte blink);

#endif /* SCREEN_H */
