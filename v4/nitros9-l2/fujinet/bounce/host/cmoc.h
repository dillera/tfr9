// cmoc.h -- for building fnbounce on Unix, for testing: the CMOC library
// calls the client uses.  It is compiled as for CMOC (_CMOC_VERSION_),
// so that it takes the same paths as on the TFR/911H.
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#ifndef BWC_HOST_CMOC_H
#define BWC_HOST_CMOC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

static inline char *itoa10(int value, char *str)
{
    sprintf(str, "%d", value);
    return str;
}

static inline char *utoa10(unsigned value, char *str)
{
    sprintf(str, "%u", value);
    return str;
}

#define itoa(value, str, base) itoa10((value), (str))
#define utoa(value, str, base) utoa10((value), (str))

#endif
