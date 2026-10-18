#ifndef TIME_H
#define TIME_H

/* -----------------------------------------------------------------------
 * time.h  –  NitrOS-9 shim for the cc65-compatible clock() interface.
 * clock() counts in sixtieths of a second from midnight, from F$Time,
 * so it moves a second at a time.
 * ----------------------------------------------------------------------- */

typedef unsigned long clock_t;

#define CLOCKS_PER_SEC  60

clock_t clock(void);

#endif /* TIME_H */
