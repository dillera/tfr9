/*
  Platform specific utilities: the move timer, from the clock (to the
  second, as sixtieths), and quitting to the shell.
  @license gpl v. 3, see fujinet/LICENSE for details.
*/

#include "../misc.h"
#include "../platform-specific/util.h"

void soundDone(void);       // sound.c

static dword timer_start;

void resetTimer()
{
    timer_start = term_seconds();
}

int getTime()
{
    dword now = term_seconds();
    if (now < timer_start)
        now += 86400UL;         // past midnight
    return (int)((now - timer_start) * 60);
}

void quit()
{
    soundDone();
    resetGraphics();
    exit(0);
}
