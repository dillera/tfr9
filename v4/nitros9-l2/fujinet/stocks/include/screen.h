#ifndef SCREEN_H
#define SCREEN_H

/* -----------------------------------------------------------------------
 * screen.h
 * Screen layout constants for the FujiNet Stocks application.
 *
 * SCREEN_WIDTH is platform-specific: 42 on CoCo (cmoc/hirestxt), 80 on
 * NitrOS-9 (the TFR/911H tether's terminal), 40 on all other platforms.
 * Everything else is derived from it so the layout automatically adapts
 * when compiled for a different target.
 *
 *   Row  0        : scrolling stock ticker
 *   Rows 1 - 21   : main content area
 *   Rows 22 - 23  : menu / command help
 * ----------------------------------------------------------------------- */
#include <stdbool.h>
#include <stdint.h>

#if defined(BUILD_OS9)
#  define SCREEN_WIDTH     80
#elif defined(_CMOC_VERSION_)
#  define SCREEN_WIDTH     42
#else
#  define SCREEN_WIDTH     40
#endif

#define SCREEN_ROWS        24
#define TICKER_ROW          0
#define MENU_ROW1          22
#define MENU_ROW2          23
#define CONTENT_TOP_ROW     1
#define CONTENT_BOT_ROW    21

/* Stock slot label geometry (used in stock_coords[] and draw_slot()).
 *
 *   Each label is: "%2d: " (4 chars) + 10-char symbol field = 14 chars.
 *   SLOT_GAP is the number of blank columns between the right edge of the
 *   left label and the left edge of the right label.
 *   The two columns are centred symmetrically within SCREEN_WIDTH.
 *
 *   Example for SCREEN_WIDTH=42:
 *     SLOT_LEFT_X  = (42 - (14+5+14)) / 2 = 4
 *     SLOT_RIGHT_X = 4 + 14 + 5           = 23
 *     Right label ends at col 36; right margin = 6
 *
 *   Example for SCREEN_WIDTH=40:
 *     SLOT_LEFT_X  = (40 - 33) / 2 = 3
 *     SLOT_RIGHT_X = 3 + 14 + 5   = 22
 *     Right label ends at col 35; right margin = 5
 */
#define SLOT_LABEL_W   14
#define SLOT_GAP        5
#define SLOT_LEFT_X    ((SCREEN_WIDTH - (2 * SLOT_LABEL_W + SLOT_GAP)) / 2)
#define SLOT_RIGHT_X   (SLOT_LEFT_X + SLOT_LABEL_W + SLOT_GAP)

/* Scroll speed: advance one character every N frames (lower = faster) */
#define SCROLL_FRAMES   6

/* Display name for the BREAK key used in menu strings.
 * CoCo has a dedicated hardware BREAK key; all other platforms use ESC
 * (on NitrOS-9, ^C too). */
#if defined(_CMOC_VERSION_) && !defined(BUILD_OS9)
#  define BREAK_KEY_NAME  "BREAK"
#else
#  define BREAK_KEY_NAME  "ESC"
#endif

void draw_centered(const char *s);
void clear_line(uint8_t row);
void draw_app_name(bool show_wait, bool show_version);

#endif /* SCREEN_H */
