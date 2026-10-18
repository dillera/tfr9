/**
 * @brief   News Reader
 * @license gpl v. 3, see LICENSE for details.
 * @verbose The screen: what cocotext.c and bar.c do for the CoCo's 80
 *          column screen, done on the tether's terminal.  bar() flipped a
 *          row's attribute bytes; here term_invert_row() flips the row in
 *          term.c's copy of the screen and sends it again.
 */

#include "globals.h"
#ifdef _CMOC_VERSION_
#include <fujinet-version.h>
#else
#define FNLIB_VERSION_FULL "(host build)"
#endif

/**
 * @brief Temp space for strupr(s) output, so original strings doesn't get changed.
 */
static char uppercase_tmp[161];

void clear_screen(byte color)
{
    (void)color;
    term_reverse(0);
    term_color(TC_DEFAULT, TC_DEFAULT);
    term_clrscr();
}

/**
 * @brief A bar across the screen, bright white on blue, the text centered
 */
void title_bar(byte y, const char *text)
{
    int pad = (textMode - (int)strlen(text)) / 2;

    gotoxy(0, y);
    term_color(TC_BRIGHT + TC_WHITE, TC_BLUE);
    printf("%*s%s%*s", pad, "", text, (int)textMode - pad - (int)strlen(text), "");
    term_color(TC_DEFAULT, TC_DEFAULT);
}

/**
 * @brief A blue rule of len dashes at the start of row y
 */
void rule(byte y, byte len)
{
    gotoxy(0, y);
    term_color(TC_BLUE, TC_DEFAULT);
    while (len--)
        putchar('-');
    term_color(TC_DEFAULT, TC_DEFAULT);
}

/**
 * @brief About the program and the server, from row y
 */
void show_about(byte y)
{
    rule(y, textMode);
    gotoxy(0, y + 2);
    term_color(TC_BRIGHT + TC_YELLOW, TC_DEFAULT);
    printf("  FujiNet News for NitrOS-9");
    term_color(TC_CYAN, TC_DEFAULT);
    gotoxy(0, y + 4);
    printf("%18s", "News server: ");
    term_color(TC_BRIGHT + TC_WHITE, TC_DEFAULT);
    printf("%s", NEWS_SERVER);
    term_color(TC_CYAN, TC_DEFAULT);
    gotoxy(0, y + 5);
    printf("%18s", "Server version: ");
    term_color(TC_BRIGHT + TC_WHITE, TC_DEFAULT);
    printf("the back end of December 2024 (it reports no number)");
    term_color(TC_CYAN, TC_DEFAULT);
    gotoxy(0, y + 6);
    printf("%18s", "Client: ");
    term_color(TC_BRIGHT + TC_WHITE, TC_DEFAULT);
    printf("fujinet-news CoCo client b351d24, fujinet-lib %s", FNLIB_VERSION_FULL);
    gotoxy(0, y + 8);
    term_color(TC_BRIGHT + TC_GREEN, TC_DEFAULT);
    printf("  Ported to NitrOS-9 in 2026 by DILLER for the TFR/911H.");
    term_color(TC_DEFAULT, TC_DEFAULT);
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
        // The keys black on cyan, the words about them in cyan.
        for (int i = 0; i < len; i++)
        {
            if (text[i] >= 'a' && text[i] <= 'z')
            {
                term_color(TC_BLACK, TC_CYAN);
                putchar(text[i] - 32);
            }
            else
            {
                term_color(TC_CYAN, TC_DEFAULT);
                putchar(text[i]);
            }
        }
        term_color(TC_DEFAULT, TC_DEFAULT);
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
