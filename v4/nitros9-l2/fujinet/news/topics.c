/**
 * @brief   News Reader
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Topics menu
 */

#include "globals.h"
#include "topics.h"

#define MENU_Y_24 8

TopicState topicState = TOPICS_RESET;

Topic selectedTopic = TOP_STORIES;

int topicChanged = 0; // flag for updating selection bar

/**
 * @brief Reset topics menu
 */
TopicState topics_reset(void)
{
    selectedTopic = TOP_STORIES;
    topicChanged = 1;
    return TOPICS_DISPLAY;
}

TopicState topics_display(void)
{
    // A block of color by each topic, as on the CoCo's 32 column screen.
    static const byte topic_colors[9] = {
        TC_RED, TC_YELLOW, TC_GREEN, TC_CYAN, TC_BLUE,
        TC_MAGENTA, TC_WHITE, TC_RED, TC_GREEN
    };

    clear_screen(1);

    title_bar(0, "FUJINET  NEWS TOPICS");
    gotoxy(0, 2);
    term_color(TC_CYAN, TC_DEFAULT);
    printf("%*s", (int)(textMode + 38) / 2, "The day's stories, through the FujiNet");
    rule(MENU_Y_24 - 2, textMode);

    for (byte i = 0; i < 9; i++)
    {
        gotoxy(0, i + MENU_Y_24);
        term_color(TC_DEFAULT, topic_colors[i]);
        printf("  ");
        term_color(TC_BRIGHT + TC_WHITE, TC_DEFAULT);
        printf("%-*s", (int)(textMode - 3), topicStrings[i]);
    }

    rule(MENU_Y_24 + 10, textMode);
    print_reverse(0, 22, "esc QUIT   i ABOUT", true);

    gotoxy(0, 23);
    term_color(TC_YELLOW, TC_DEFAULT);
    printf("%-*s", (int)(textMode - 1), "SELECT WITH ARROWS, THEN ENTER.");
    term_color(TC_DEFAULT, TC_DEFAULT);

    return TOPICS_MENU;
}

TopicState topics_menu()
{
    if (topicChanged)
    {
        topicChanged = 0;
        bar((byte)selectedTopic + MENU_Y_24);
        gotoxy(textMode - 1, 23);
    }

    while (true)
    {
        switch (waitkey(false))
        {
        case ARROW_UP:
            return TOPICS_UP;
        case ARROW_DOWN:
            return TOPICS_DOWN;
        case ENTER:
            return TOPICS_SELECT;
        case BREAK:
            return TOPICS_EXIT;
        case 'I':
        case 'i':
            clear_screen(1);
            title_bar(0, "FUJINET  NEWS");
            show_about(4);
            gotoxy((textMode - 27) >> 1, 22);
            printf("<Press any key to continue>");
            waitkey(1);
            topicChanged = 1;
            return TOPICS_DISPLAY;
        }
    }
}

TopicState topics_up()
{
    if (selectedTopic > TOP_STORIES)
    {
        bar((byte)selectedTopic + MENU_Y_24);
        gotoxy(textMode - 1, 23);
        selectedTopic--;
        topicChanged = 1;
    }

    return TOPICS_MENU;
}

TopicState topics_down()
{
    if (selectedTopic < SPORTS)
    {
        bar((byte)selectedTopic + MENU_Y_24);
        gotoxy(textMode - 1, 23);
        selectedTopic++;
        topicChanged = 1;
    }

    return TOPICS_MENU;
}

State topics(void)
{
    topicState=TOPICS_RESET;

    while (state == TOPICS)
    {
        switch(topicState)
        {
        case TOPICS_RESET:
            topicState = topics_reset();
            break;
        case TOPICS_DISPLAY:
            topicState = topics_display();
            break;
        case TOPICS_MENU:
            topicState = topics_menu();
            break;
        case TOPICS_UP:
            topicState = topics_up();
            break;
        case TOPICS_DOWN:
            topicState = topics_down();
            break;
        case TOPICS_SELECT:
            return ARTICLES;
        case TOPICS_EXIT:
            return BYE;
        }
    }

    return BYE;
}
