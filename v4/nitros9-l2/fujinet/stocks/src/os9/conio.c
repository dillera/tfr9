/* -----------------------------------------------------------------------
 * conio.c  –  NitrOS-9: keys and line input on the tether's terminal
 * (../../../c/term.c).
 *
 * kbhit() waits a tick when no key is waiting.  That paces the main loop,
 * and so the ticker, at 60 Hz, as sync_frame() does on other platforms,
 * and keeps the loops that wait for a key from taking the 6309 from every
 * other process; sync_frame() is empty here.
 * ----------------------------------------------------------------------- */

#include <conio.h>
#include <get_line.h>
#include "stocks.h"
#include "init.h"

static char pending;        /* the key kbhit() saw, for cgetc() */

/**
 * @brief Report whether a key is waiting, without taking it.
 *
 * @return true if cgetc() will return at once.
 */
bool kbhit(void)
{
    if (pending)
        return true;
    if (term_ready()) {
        pending = (char)term_getc();
        if (pending)
            return true;
    }
    term_sleep(1);
    return false;
}

/**
 * @brief Wait for a key.
 *
 * @return The key (see chardefs.h).
 */
char cgetc(void)
{
    char c = pending;

    pending = 0;
    while (!c)
        c = (char)term_getc();
    return c;
}

/**
 * @brief Read a line of input with echo and backspace; BREAK leaves it empty.
 *
 * @param buf     Output buffer for the entered string.
 * @param max_len Maximum number of characters (including NUL terminator).
 */
void get_line(char *buf, uint8_t max_len)
{
    term_getline(buf, max_len);
}

/**
 * @brief Nothing: kbhit() paces the main loop.
 */
void sync_frame(void)
{
}

/**
 * @brief Report whether the platform clears the screen on exit.
 *
 * @return Always 1: term_done() clears it.
 */
uint8_t doesclrscrafterexit(void)
{
    return 1;
}
