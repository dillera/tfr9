// fntypes.h -- the basic types of the FujiNet C programs, for CMOC on
// NitrOS-9 (with the same guards as CMOC's coco.h) and for testing on Unix.

#ifndef FNTYPES_H
#define FNTYPES_H

#ifdef _CMOC_VERSION_

#include <cmoc.h>
#include <stdarg.h>

#ifndef _CMOC_HAVE_FALSE_TRUE_
enum { FALSE, TRUE };
#define _CMOC_HAVE_FALSE_TRUE_
#endif

#ifndef _CMOC_HAVE_BOOL_
typedef unsigned char BOOL;
#define _CMOC_HAVE_BOOL_
#endif

#ifndef _CMOC_BASIC_TYPES_
#define _CMOC_BASIC_TYPES_
typedef unsigned char byte;
typedef signed char   sbyte;
typedef unsigned int  word;
typedef signed int    sword;
typedef unsigned long dword;
typedef signed long   sdword;
#endif

#ifndef _CMOC_STDINT_
#define _CMOC_STDINT_
typedef unsigned char uint8_t;
typedef signed char   int8_t;
typedef unsigned int  uint16_t;
typedef signed int    int16_t;
typedef unsigned long uint32_t;
typedef signed long   int32_t;
#endif

#ifndef FN_HAVE_BOOL
#define FN_HAVE_BOOL
typedef unsigned char bool;
#define true 1
#define false 0
#endif

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
