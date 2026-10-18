/**
 * @brief   News Reader
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Main Program
 *
 * fnnews: the CoCo client of FujiNetWIFI/fujinet-news (clients/src/coco,
 * commit b351d24), for NitrOS-9 Level 2 on the TFR/911H.  It keeps the
 * CoCo's 80 column layout, on the tether's terminal (screen.c), with Esc
 * for BREAK; it has no colour schemes or screen widths to choose, so no
 * settings to keep in an app key.
 */

#include "globals.h"
#include "topics.h"
#include "articles.h"
#include "article.h"

void init(void)
{
    term_init();
    textMode = TERM_COLS;
}

int main(void)
{
    init();

    while(1)
    {
        switch(state)
        {
        case TOPICS:
            state=topics();
            break;
        case ARTICLES:
            state=articles();
            break;
        case ARTICLE:
            state=article();
            break;
        case BYE:
            break;
        }

        if (state==BYE)
            break;
    }

    term_done();

    return 0;
}
