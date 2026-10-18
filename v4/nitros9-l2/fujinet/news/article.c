/**
 * @brief   News Reader
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Article display
 */

#include "globals.h"
#include "article.h"
#include "network.h"

/**
 * @brief Article Page buffer
 */
static char *article_page_buffer;

/**
 * @brief Article metadata
 */
static char *title = NULL;
static char *date = NULL;
static char *source = NULL;
static char *page = NULL;
static char *pageData = NULL;
static char none[1];
static int page_max = 0;
static byte menu_line = 22; // line for menu display

/**
 * @brief current state for article view
 */
static ArticleState articleState;

static char menu_line_buffer[81];

char *articles_display_headline(char *s);

/**
 * @brief The headline across the top two rows, bright white on blue
 */
static void article_headline(void)
{
    gotoxy(0, 0);
    term_color(TC_BRIGHT + TC_WHITE, TC_BLUE);
    printf("%-*s", (int)textMode * 2, articles_display_headline(title));
    term_color(TC_DEFAULT, TC_DEFAULT);
}

/**
 * @brief Initial entrypoint, reset local state
 */
ArticleState article_reset(void)
{
    article_page = 1;
    menu_line = 22;
    rows = 17;

    return ARTICLE_FETCH;
}

/**
 * @brief Fetch current article page
 */
ArticleState article_fetch(void)
{
    char *p = NULL;
    char tmp[8];


    strcpy(fetching_buf, "FETCHING PAGE, PLEASE WAIT.");

    show_fetching_msg(true, false);

    article_page_buffer = fetch_data(false);

    title = strtok(article_page_buffer,"\n");
    date = strtok(NULL,"\n");
    source = strtok(NULL,"\n");
    page = strtok(NULL,"\n");
    pageData = strtok(NULL,"\0");

    // A page that did not come leaves these empty, not NULL.
    if (!title) title = none;
    if (!date) date = none;
    if (!source) source = none;
    if (!page) page = none;
    if (!pageData) pageData = none;

    // copy, parse page counter so we can get max # of pages.
    strncpy(tmp,page,sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    p = strtok(tmp,"/"); // skip over current page
    p = strtok(NULL,"\0"); // get last # as max pages.
    page_max = p ? atoi(p) : 1; // set it.

    return ARTICLE_DISPLAY;
}

/**
 * @brief Display current article page
 */
ArticleState article_display(void)
{
    clear_screen(1);

    article_headline();
    gotoxy(0, 2);
    term_color(TC_YELLOW, TC_DEFAULT);
    printf("%s   %s", date, source);
    rule(3, textMode);

    // The text starts where a CoCo puts it after "\n\n", the headline
    // having taken it to the start of row 2.
    gotoxy(0, 4);
    term_color(TC_DEFAULT, TC_DEFAULT);
    term_puts(pageData);    // longer than printf() can take
    rule(menu_line - 1, textMode);

    return ARTICLE_MENU;
}

/**
 * @brief Format a menu line to fit in given width
 * @param page Current page string
 * @param text Menu text
 * @param width Width of the line
 * @return pointer to static buffer containing formatted line
 */
char *format_menu_line(const char *page, const char *menu_text, int width)
{
    memset(menu_line_buffer, 0, sizeof(menu_line_buffer));

    // Start with spaces
    memset(menu_line_buffer, ' ', width);
    menu_line_buffer[width] = '\0';

    // Place page string on the left
    int page_len = strlen(page);
    if (page_len > width) page_len = width;
    memcpy(menu_line_buffer, page, page_len);

    // Center the menu text
    int menu_len = strlen(menu_text);
    if (menu_len > width) menu_len = width;

    int menu_start = (width - menu_len) / 2;
    memcpy(menu_line_buffer + menu_start, menu_text, menu_len);

    return menu_line_buffer;
}

/**
 * @brief Handle article menu
 */
ArticleState article_menu(void)
{
    print_reverse(0, menu_line, format_menu_line(page, "up/down iNFO  esc ARTICLES", textMode), true);

    gotoxy(textMode -1,menu_line);

    switch(waitkey(0))
    {
    case BREAK:
        return ARTICLE_EXIT;
    case ARROW_UP:
        return ARTICLE_PREV_PAGE;
    case ARROW_DOWN:
        return ARTICLE_NEXT_PAGE;
    case 'I':
    case 'i':
        return ARTICLE_INFO;
    }

    return ARTICLE_MENU;
}

/**
 * @brief Go to next page
 */
ArticleState article_next_page(void)
{
    article_page++;

    if (article_page > page_max)
    {
        article_page = page_max;
        return ARTICLE_MENU;
    }

    return ARTICLE_FETCH;
}

/**
 * @brief Go to previous page
 */
ArticleState article_prev_page(void)
{
    if (article_page == 1)
        return ARTICLE_MENU;

    article_page--;

    return ARTICLE_FETCH;
}

/**
 * @brief Show Article Metadata
 */
ArticleState article_info(void)
{
    clear_screen(1);

    article_headline();
    gotoxy(0, 4);
    term_color(TC_CYAN, TC_DEFAULT);
    printf("%18s", "Date: ");
    term_color(TC_BRIGHT + TC_WHITE, TC_DEFAULT);
    printf("%s", date);
    gotoxy(0, 5);
    term_color(TC_CYAN, TC_DEFAULT);
    printf("%18s", "Source: ");
    term_color(TC_BRIGHT + TC_WHITE, TC_DEFAULT);
    printf("%s", source);
    show_about(8);

    gotoxy((textMode - 27) >> 1, menu_line);
    printf("<Press any key to continue>");

    waitkey(1);

    return ARTICLE_DISPLAY;
}

/**
 * @brief Handle article state
 */
State article(void)
{
    articleState=ARTICLE_RESET;

    while (state == ARTICLE)
    {
        switch(articleState)
        {
        case ARTICLE_RESET:
            articleState = article_reset();
            break;
        case ARTICLE_FETCH:
            articleState = article_fetch();
            break;
        case ARTICLE_DISPLAY:
            articleState = article_display();
            break;
        case ARTICLE_MENU:
            articleState = article_menu();
            break;
        case ARTICLE_NEXT_PAGE:
            articleState = article_next_page();
            break;
        case ARTICLE_PREV_PAGE:
            articleState = article_prev_page();
            break;
        case ARTICLE_INFO:
            articleState = article_info();
            break;
        case ARTICLE_EXIT:
            return ARTICLES;
        }
    }

    return ARTICLES;
}
