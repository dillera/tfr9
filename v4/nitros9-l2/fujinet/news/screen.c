/**
 * @brief   News Reader
 * @license gpl v. 3, see LICENSE for details.
 * @verbose The screen: what cocotext.c and bar.c do for the CoCo's 80
 *          column screen, done on the tether's terminal.  bar() flipped a
 *          row's attribute bytes; here term_invert_row() flips the row in
 *          term.c's copy of the screen and sends it again.
 */

#include "globals.h"

/**
 * @brief Temp space for strupr(s) output, so original strings doesn't get changed.
 */
static char uppercase_tmp[161];

void clear_screen(byte color)
{
    (void)color;
    term_reverse(0);
    term_clrscr();
}

void bar(byte y)
{
    term_invert_row(y);
}

/**
 * @brief Print text at x, y; with lowercase_only, the lowercase letters (the
 * keys to press) in reverse video and upper case, the rest as it is.
 */
void print_reverse(byte x, byte y, const char *text, bool lowercase_only)
{
    int len = strlen(text);

    gotoxy(x, y);

    if (lowercase_only)
    {
        for (int i = 0; i < len; i++)
        {
            if (text[i] >= 'a' && text[i] <= 'z')
            {
                term_reverse(1);
                putchar(text[i] - 32);
            }
            else
            {
                term_reverse(0);
                putchar(text[i]);
            }
        }
        term_reverse(0);
    }
    else
    {
        term_reverse(1);
        term_puts(text);
        term_reverse(0);
    }
}

/**
 * @brief Return uppercase string without modifying original
 */
char *screen_upper(const char *s)
{
    strncpy(uppercase_tmp, s, sizeof(uppercase_tmp) - 1);
    uppercase_tmp[sizeof(uppercase_tmp) - 1] = '\0';
    return strupr(uppercase_tmp);
}

/**
 * @brief Return lowercase string without modifying original
 */
char *screen_lower(const char *s)
{
    strncpy(uppercase_tmp, s, sizeof(uppercase_tmp) - 1);
    uppercase_tmp[sizeof(uppercase_tmp) - 1] = '\0';
    return strlwr(uppercase_tmp);
}

void show_fetching_msg(bool clear, bool articles)
{
    (void)articles;
    if (clear)
    {
        clear_screen(1);
    }
    gotoxy(0, 0);
    printf("%-*s", (int)(textMode - 1), fetching_buf);
}

void get_line(char *buf, uint8_t max_len)
{
    term_getline(buf, max_len);
}

byte waitkey(byte blink)
{
    (void)blink;
    return term_getc();
}
