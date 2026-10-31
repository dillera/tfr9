/*
  The title screen: "TEXAS HOLD'EM" in big letters over a fanned royal
  flush and two stacks of chips, on the table's felt, for three seconds
  or until a key.  title.py draws it and writes title.h, the cells to
  put; they go through screen.c like the rest, so the felt shows through
  wherever nothing is drawn, and the welcome screen after it (which keeps
  the letters, as drawLogo()) sends only what changes.
  @license gpl v. 3, see fujinet/LICENSE for details.
*/

#include "../misc.h"
#include "../screens.h"
#include "table.h"
#include "title.h"

extern const char build_date[];
extern const char upstream_version[];

// The cells of a list in title.h (see title.py)
static void drawArt(const char *p)
{
    byte x = 0, y = 0, fg = C_TEXT, bg = C_KEEP, c, n;
    while ((c = (byte)*p++) != 0) {
        switch (c) {
        case 1:
            x = (byte)(p[0] - 1);
            y = (byte)(p[1] - 1);
            p += 2;
            break;
        case 2:
            fg = (byte)*p++;
            break;
        case 3:
            bg = (byte)*p++;
            break;
        case 5:
            bg = C_KEEP;
            break;
        case 4:
            n = (byte)p[0];
            c = (byte)p[1];
            p += 2;
            scr_ink(fg, bg);
            while (n--)
                scr_put(x++, y, c);
            break;
        default:
            scr_ink(fg, bg);
            scr_put(x++, y, c);
            break;
        }
    }
}

void drawTitleLetters(void)
{
    drawArt(TITLE_LETTERS);
}

void showTitleScreen()
{
    byte t, x;
    resetScreen();
    drawBorder();
    drawArt(TITLE_SHADOW);
    scr_present();
    drawTitleLetters();
    drawArt(TITLE_FAN);
    drawCaps(33, 19, "A FUJINET GAME", C_DIM, 255);

    clearStatusBar();
    drawStatusTextAt(34, "PRESS A KEY");
    x = drawCaps(WIDTH - 31, ROW_KEYS, "FNHOLDEM ", C_DIM, 255);
    x = drawCaps(x, ROW_KEYS, upstream_version, C_DIM, 8);
    drawCaps((byte)(x + 1), ROW_KEYS, build_date, C_DIM, 10);
    scr_present();

    for (t = 0; t < 180 && !term_ready(); t++)
        term_sleep(1);
    if (term_ready())
        term_getc();
}
