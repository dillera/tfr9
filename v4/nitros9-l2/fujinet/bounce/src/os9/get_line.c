#include "conio_wrapper.h"
#include "get_line.h"

// Up to max_len - 1 characters, as on the other platforms; BREAK leaves
// the line empty.
void get_line(char *buf, uint8_t max_len)
{
	term_getline(buf, max_len);
}
