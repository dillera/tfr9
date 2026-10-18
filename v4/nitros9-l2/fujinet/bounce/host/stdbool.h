// stdbool.h -- for building fnbounce on Unix, for testing: bool as CMOC
// has it, a byte, since the client declares some flags as bool and
// defines them as uint8_t.

#ifndef BWC_HOST_STDBOOL_H
#define BWC_HOST_STDBOOL_H

typedef unsigned char _FNBool;
#define bool _FNBool
#define true 1
#define false 0

#endif
