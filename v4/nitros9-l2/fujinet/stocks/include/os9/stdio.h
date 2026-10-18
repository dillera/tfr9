#ifndef STDIO_H
#define STDIO_H

/* NitrOS-9: printf() and putchar() on the tether's terminal (term.c). */

#include "term.h"

#define printf term_printf
#define putchar term_putc
#define fflush(stream) /* no-op */
#define snprintf(buf, len, ...) sprintf(buf, __VA_ARGS__)

#endif /* STDIO_H */
