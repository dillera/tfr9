// vars.h -- Texas Hold'em on NitrOS-9: the tether's terminal, 80 by 24
// in xterm's 256 colours (src/os9/screen.c), and the keys term.c reads.
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#ifndef KEYMAP_H
#define KEYMAP_H

#include "term.h"

#define WIDTH 80
#define HEIGHT 24

// The move countdown ends the move row: the seconds, then an "s".
#define STATUS_TIMER_WIDTH 2
#define PLAYER_MOVE_START_X 2

// The instructions fit between the rails (rows 2 to 20)
#define HOW_TO_PLAY_ROW_START 2
#define SKIP_FIRST_BUMP

// colorMode 0, the green felt (src/os9/graphics.c has the others)
#define DEFAULT_COLOR_PREF 1

// The server sends its numbers little-endian unless asked: the 6809
// wants them big-endian, as the CoCo does.  The Unix build, for testing,
// takes them as they come.
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define QUERY_SUFFIX ""
#else
#define QUERY_SUFFIX "&be=1"
#endif

/**
 * Platform specific key map for common input: term.c's keys.  The
 * other two sets are control keys that nothing else uses (and below
 * 128, as inputKey is a signed char).
 */
#define KEY_LEFT_ARROW      KEY_LEFT
#define KEY_LEFT_ARROW_2    0x1C
#define KEY_LEFT_ARROW_3    0x12
#define KEY_RIGHT_ARROW     KEY_RIGHT
#define KEY_RIGHT_ARROW_2   0x1D
#define KEY_RIGHT_ARROW_3   0x14
#define KEY_UP_ARROW        KEY_UP
#define KEY_UP_ARROW_2      0x1E
#define KEY_UP_ARROW_3      0x17
#define KEY_DOWN_ARROW      KEY_DOWN
#define KEY_DOWN_ARROW_2    0x1F
#define KEY_DOWN_ARROW_3    0x19

#define KEY_RETURN       KEY_ENTER
#define KEY_ESCAPE       KEY_BREAK  // Esc or ^C
#define KEY_ESCAPE_ALT   0x1B
#define KEY_SPACE        0x20
#define KEY_BACKSPACE    KEY_BS

#endif /* KEYMAP_H */
