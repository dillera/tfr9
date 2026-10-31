/*
  Platform specific input: the keyboard, through term.c.  The TFR/911H has
  no joystick.
  @license gpl v. 3, see fujinet/LICENSE for details.
*/

#include "../misc.h"

unsigned char kbhit(void)
{
    return term_ready();
}

char cgetc(void)
{
    return (char)term_getc();
}

unsigned char readJoystick()
{
    return 0;
}
