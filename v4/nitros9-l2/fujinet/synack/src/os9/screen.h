// screen.h -- SYN-ACK WIRE's drawing on the tether's terminal: ANSI, in
// xterm's 256 colours, through fujinet/c/term.c (which also reads the
// keys).  Colours are sent only when a character needs them changed.
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#ifndef SCREEN_H
#define SCREEN_H

#define SCR_COLS 80
#define SCR_ROWS 24

#define C_NONE 0                // the terminal's own colour (xterm's 0, black, is 16)

void scr_init(void);            // raw keys, cursor off, the screen cleared
void scr_done(void);            // the terminal as it was
void scr_clear(void);
void scr_at(unsigned char row, unsigned char col);
void scr_ink(unsigned char fg, unsigned char bg);
void scr_bold(unsigned char on);
void scr_put(char c);           // one character; \ is the yen sign
void scr_text(const char *s);
void scr_textn(const unsigned char *s, unsigned char n);
void scr_spaces(unsigned char n);
void scr_glyph(const char *utf8);   // one character cell, in UTF-8
void scr_eol(void);             // erase to the end of the row, in the background colour
void scr_erase_row(void);       // the whole row, the same way
void scr_raw(const char *s);    // escape sequences and the like, as they are
void scr_sync(void);            // the terminal to scr_ink()'s colours now, before scr_raw()
void scr_reset(void);           // colours and bold back to the terminal's
void scr_flush(void);

#endif
