/*
  Platform specific utilities: the move timer, from the clock (to the
  second, as sixtieths), a random number, and quitting to the shell.
  @license gpl v. 3, see fujinet/LICENSE for details.
*/

#include "../misc.h"

static dword timer_start;

void resetTimer()
{
    timer_start = term_seconds();
}

uint16_t getTime()
{
    dword now = term_seconds();
    if (now < timer_start)
        now += 86400UL;         // past midnight
    return (uint16_t)((now - timer_start) * 60);
}

void quit()
{
    resetGraphics();
    exit(0);
}

void housekeeping()
{
}

uint8_t getJiffiesPerSecond()
{
    return 60;
}

uint8_t getRandomNumber(uint8_t maxExclusive)
{
    static byte seeded;
    if (!seeded) {
        srand((unsigned)term_seconds());
        seeded = 1;
    }
    return (uint8_t)(rand() % maxExclusive);
}
