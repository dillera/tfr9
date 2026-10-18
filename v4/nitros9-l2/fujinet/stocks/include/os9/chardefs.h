#ifndef CHARDEFS_H
#define CHARDEFS_H

/* -----------------------------------------------------------------------
 * chardefs.h  –  NitrOS-9 key code definitions: the keys term_getc()
 * returns (term.h).  Esc and ^C are BREAK.
 * ----------------------------------------------------------------------- */

#include "term.h"

#define ARROW_UP    KEY_UP
#define ARROW_DOWN  KEY_DOWN
#define ARROW_LEFT  KEY_LEFT
#define ARROW_RIGHT KEY_RIGHT
#define ENTER       KEY_ENTER
#define BREAK       KEY_BREAK
#define NEWLINE     0x0A

#endif /* CHARDEFS_H */
