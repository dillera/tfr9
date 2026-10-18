#include "conio_wrapper.h"
#include "double_buffer.h"

// The CoCo draws each frame on the screen it is not showing, then shows
// it.  term.c draws a frame in its copy of the screen alone, then sends
// the terminal only the cells that changed.

uint8_t is_alt_screen;
extern byte in_simulation;

void swap_buffer()
{
	in_simulation = 1;
	term_frame(1);
}

void show_other_screen()
{
	term_frame(0);
	term_flush();
}
