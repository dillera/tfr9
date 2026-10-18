#include "data.h"
#include "conio_wrapper.h"
#include "collision.h"
#include "sound.h"

void collision_fx()
{
    if (play_sound_on_collision)
    {
        sound_collision();
    }
    if (flash_on_collision)
    {
        // Flash the screen into reverse video and back, twice.
        for (int i = 0; i < 4; i++)
        {
            switch_colorset();
            term_sleep(5);
        }
    }
}
