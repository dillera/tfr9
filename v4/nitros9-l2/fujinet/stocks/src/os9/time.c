/* -----------------------------------------------------------------------
 * time.c  –  NitrOS-9 clock() implementation.
 *
 * Counts from midnight, from the seconds of F$Time, in the sixtieths of
 * CLOCKS_PER_SEC (include/os9/time.h).  At midnight the count starts
 * again, and the difference the main loop takes is then huge, so it
 * fetches new quotes.
 * ----------------------------------------------------------------------- */

#include <time.h>
#include "term.h"

/**
 * @brief Return sixtieths of a second since midnight.
 *
 * @return clock() ticks, CLOCKS_PER_SEC a second.
 */
clock_t clock(void)
{
    return (clock_t)term_seconds() * CLOCKS_PER_SEC;
}
