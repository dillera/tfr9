// fntypes.h -- the basic types of the FujiNet C programs: fujinet-lib's,
// for CMOC on NitrOS-9, and the same for testing on Unix.

#ifndef FNTYPES_H
#define FNTYPES_H

#ifdef _CMOC_VERSION_

// fujinet-lib's: cmoc.h (with stdarg.h), coco.h (byte, word, BOOL,
// uint8_t ...), and bool.
#include <fujinet-int.h>

#else // Unix, for testing

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>

typedef unsigned char byte;
typedef signed char sbyte;
typedef uint16_t word;
typedef int16_t sword;
typedef uint32_t dword;
typedef unsigned char BOOL;
#define TRUE 1
#define FALSE 0

char *strupr(char *s);
char *strlwr(char *s);
#define stricmp strcasecmp

#endif

#endif // FNTYPES_H
