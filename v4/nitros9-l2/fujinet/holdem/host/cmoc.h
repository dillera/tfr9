// cmoc.h -- for building fnholdem on Unix, for testing: the CMOC library
// calls the client uses.  It is compiled as for CMOC (_CMOC_VERSION_),
// so that it takes the same paths as on the TFR/911H.
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#ifndef FNHOLDEM_HOST_CMOC_H
#define FNHOLDEM_HOST_CMOC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>

static inline char *itoa10(int value, char *str)
{
    sprintf(str, "%d", value);
    return str;
}

#define itoa(value, str, base) itoa10((value), (str))

// CMOC lays a structure out with no padding, and the server's state comes
// in that way (src/misc.h): so must everything after this here.
#pragma pack(1)

#endif
