// stdbool.h -- for building fnholdem on Unix, for testing: bool as CMOC
// has it, a byte, since the client declares some flags as bool and
// others as unsigned char.
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#ifndef FNHOLDEM_HOST_STDBOOL_H
#define FNHOLDEM_HOST_STDBOOL_H

typedef unsigned char _FNBool;
#define bool _FNBool
#define true 1
#define false 0

#endif
