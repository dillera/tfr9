// @license gpl v. 3, see fujinet/LICENSE for details.
#ifndef KEYMAP_H
#define KEYMAP_H

// NitrOS-9 on the TFR/911H: the tether's terminal, 80 by 24, in color.

#include "term.h"
#include "glyphs.h"

#define WIDTH 80
#define HEIGHT 24

#define GAMEOVER_PROMPT_Y HEIGHT - 2

#define ESCAPE "ESC"
#define ESC "ESC"

// The prompts, for a keyboard
#define READY_PROMPT_TEXT "press SPACE when ready"
#define MENU_CLOSE_TEXT "press SPACE to close"
#define TABLE_STATUS_TEXT "Refresh    Help    Name    Sound    Quit"
#define ROTATE_PROMPT_TEXT "press R to rotate"

// Icons, which drawIcon() draws in color
#define ICON_TEXT_CURSOR 1
#define ICON_PLAYER 2
#define ICON_MARK 3
#define ICON_MARK_ALT 4
#define ICON_ACTIVE_PLAYER 5

/**
 * Platform specific key map for common input: term.c's keys.  The
 * other two sets cannot be typed (digits are rows: see gamelogic.c).
 */

#define KEY_LEFT_ARROW KEY_LEFT
#define KEY_LEFT_ARROW_2 0xE1
#define KEY_LEFT_ARROW_3 0xF1

#define KEY_RIGHT_ARROW KEY_RIGHT
#define KEY_RIGHT_ARROW_2 0xE2
#define KEY_RIGHT_ARROW_3 0xF2

#define KEY_UP_ARROW KEY_UP
#define KEY_UP_ARROW_2 0xE3
#define KEY_UP_ARROW_3 0xF3

#define KEY_DOWN_ARROW KEY_DOWN
#define KEY_DOWN_ARROW_2 0xE4
#define KEY_DOWN_ARROW_3 0xF4

#define KEY_RETURN KEY_ENTER

#define KEY_ESCAPE KEY_BREAK
#define KEY_ESCAPE_ALT 0x1B

#define KEY_SPACEBAR 0x20
#define KEY_BACKSPACE KEY_BS

/* Macros that evaluate the return code of joy_read: no joystick */
#define JOY_UP(v) ((v) & 1)
#define JOY_DOWN(v) ((v) & 2)
#define JOY_LEFT(v) ((v) & 4)
#define JOY_RIGHT(v) ((v) & 8)
#define JOY_BTN_1(v) ((v) & 16)
#define JOY_BTN_2(v) ((v) & 32)

#endif /* KEYMAP_H */
