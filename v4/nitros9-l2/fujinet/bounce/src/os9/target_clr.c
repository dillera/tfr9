#include "conio_wrapper.h"
#include "playfield_clr.h"
#include "screen.h"

// All but the two rows of the info display.
void playfield_clr() {
	byte y;

	for (y = 0; y < SCREEN_HEIGHT - 2; y++) {
		term_gotoxy(0, y);
		term_clreol();
	}
}
