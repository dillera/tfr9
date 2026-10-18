#include "conio_wrapper.h"
#include "shutdown.h"
#include "sound.h"

void cleanup_client()
{
    stop_sound();
    term_raw("\033[?5l");   // not in reverse video ('C')
    term_done();
}
