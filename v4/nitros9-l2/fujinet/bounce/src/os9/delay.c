#include "conio_wrapper.h"
#include "delay.h"

void pause(uint8_t count)
{
	term_sleep(count);
}

void wait_vsync()
{
	term_sleep(1);
}
