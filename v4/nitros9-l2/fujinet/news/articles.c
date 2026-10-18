/**
 * @brief   FujiNet News Reader
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Articles Display/Select
 */

#include "globals.h"
#include "articles.h"
#include "network.h"

#define MAX_ARTICLES_PER_PAGE 6

static ArticlesState articlesState = ARTICLES_RESET;
static char *articles_buffer;
static char outs[161];
static char topic_line[81];
byte menu_line = 22;
static int headline_length = 160;
static int articles_on_this_page = 0;
static char page[32];   // enough for "12345/12345" etc.
static char article_cursor_pos = 0;
static char article_cursor_pos_prev = 0;

extern long article_id;

struct Article {
    long id;             // use long for article ID (at least 32 bits)
    char timestamp[20];  // "YYYY-MM-DD HH:MM:SS"
    char headline[160];   // max headline_length in any mode
};

struct Article _articles[MAX_ARTICLES_PER_PAGE];

static int parse_articles_response(char *input, struct Article *articles, int capacity);

const char *category_num_to_name(int c)
{
    return strtok(screen_lower(topicStrings[c]), " ");
};

const byte headline_locations_80[6] = {1, 4, 7, 10, 13, 16};


/**
 * @brief Reset articles state
 */
ArticlesState articles_reset(void)
{
    articles_page = 1;
    article_cursor_pos = 0;
    article_cursor_pos_prev = 0;

    menu_line = 22;
    rows = 21;
    articles_per_page = 6;
    headline_length = 160;

    memset(_articles, 0, MAX_ARTICLES_PER_PAGE * sizeof(struct Article));

    return ARTICLES_FETCH;
}

/**
 * @brief Fetch articles for current page
 */
ArticlesState articles_fetch(void)
{
    strcpy(fetching_buf, "FETCHING ARTICLES, PLEASE WAIT.");

    topic = category_num_to_name(selectedTopic);

    show_fetching_msg(true, true);

    articles_buffer = fetch_data(true);
    articles_on_this_page = parse_articles_response(articles_buffer, _articles, MAX_ARTICLES_PER_PAGE);

    return ARTICLES_DISPLAY;
}

/**
 * @brief Transform and display article headline
 * @param s headline string
 */
char *articles_display_headline(char *s)
{
    memset(outs,0,sizeof(outs));

    strncpy(outs,s,headline_length);

    // Ellipsize if needed.
    if (strlen(outs)==headline_length)
    {
        outs[headline_length-3]=outs[headline_length-2]=outs[headline_length-1]='.';
    }

    return outs;
}

char *format_topic_line(const char *page, const char *title, int width) {

    memset(topic_line, 0, sizeof(topic_line));

    // Start with spaces
    memset(topic_line, ' ', width);
    topic_line[width] = '\0';

    // Place page string on the left
    int page_len = strlen(page);
    if (page_len > width) page_len = width;
    memcpy(topic_line, page, page_len);

    // Center the title
    int title_len = strlen(title);
    if (title_len > width) title_len = width;

    int title_start = (width - title_len) / 2;
    memcpy(topic_line + title_start, title, title_len);

    return topic_line;
}


/**
 * @brief _articles[ the articles response
 * @param input API response string
 * @param articles caller-provided buffer of struct Article[]
 * @param capacity max number of articles that fit in the buffer
 * @return Number of artticles on the page if success, -1 on error
 */
static int parse_articles_response(char *input, struct Article *articles, int capacity)
{
    char *line;
    int article_count;

    if (!input || !*input || !articles || capacity <= 0)
        return -1;

    // initialize caller-provided array and page buffer
    memset(articles, 0, capacity * sizeof(struct Article));
    memset(page, 0, sizeof(page));

    article_count = 0;
    line = input;

    while (line && *line)
    {
        // find end of current line
        char *next_line = strchr(line, '\n');
        if (next_line)
        {
            *next_line = '\0';   // terminate current line
            next_line++;         // start of next line
        }

        // skip empty lines
        if (*line == '\0')
        {
            line = next_line;
            continue;
        }

        // first non-empty line is the page string (e.g. "1/245")
        if (page[0] == '\0')
        {
            strncpy(page, line, sizeof(page) - 1);
            page[sizeof(page) - 1] = '\0';
        }
        else
        {
            // parse article lines
            if (article_count < capacity)
            {
                char *first_pipe = strchr(line, '|');
                if (first_pipe)
                {
                    *first_pipe++ = '\0';   // id_str is now line

                    char *second_pipe = strchr(first_pipe, '|');
                    if (second_pipe)
                    {
                        *second_pipe++ = '\0'; // ts is first_pipe, headline is second_pipe

                        struct Article *a = &articles[article_count];

                        a->id = strtol(line, NULL, 10);

                        strncpy(a->timestamp, first_pipe, sizeof(a->timestamp) - 1);
                        a->timestamp[sizeof(a->timestamp) - 1] = '\0';

                        strncpy(a->headline, second_pipe, sizeof(a->headline) - 1);
                        a->headline[sizeof(a->headline) - 1] = '\0';

                        article_count++;
                    }
                }
            }
        }

        line = next_line;
    }

    return article_count;
}


/**
 * @brief Display fetched articles
 */
ArticlesState articles_display(void)
{
    byte y;

    clear_screen(1);
    gotoxy(0,0);

    if (articles_on_this_page > 0)
    {
        for (int i = 0; i < articles_on_this_page; i++)
        {
            y = headline_locations_80[i] - 1;

            // A rule between stories, ending in the story's time.
            rule(y, (byte)(textMode - strlen(_articles[i].timestamp) - 1));
            putchar(' ');
            term_color(TC_YELLOW, TC_DEFAULT);
            printf("%s", _articles[i].timestamp);
            gotoxy(0, y + 1);
            term_color(TC_BRIGHT + TC_WHITE, TC_DEFAULT);
            printf("%-*s", headline_length, articles_display_headline(_articles[i].headline));
            term_color(TC_DEFAULT, TC_DEFAULT);
        }

        title_bar(menu_line - 1, format_topic_line(page, topicStrings[selectedTopic], textMode));

        byte base = headline_locations_80[article_cursor_pos];
        for (byte r = 0; r < 2; r++)
            bar(base + r);
    }

    return ARTICLES_MENU;
}

/**
 * @brief Update articles menu bar
 */
void articles_bar(void)
{
    if (article_cursor_pos_prev != article_cursor_pos)
    {
        byte base_prev = headline_locations_80[article_cursor_pos_prev];
        byte base_cur = headline_locations_80[article_cursor_pos];
        for (byte r = 0; r < 2; r++) {
            bar(base_prev + r);
            bar(base_cur  + r);
        }
        article_cursor_pos_prev = article_cursor_pos;
    }
}

/**
 * @brief Handle menu for current article page
 */
ArticlesState articles_menu(void)
{
    const char *l1 = "up/down  CHOOSE  esc  TOPICS";
    const char *l2 = "left/right  PAGE     enter  VIEW  gO TO PAGE";

    print_reverse((byte)((textMode - strlen(l1)) >> 1), menu_line, l1, true);
    print_reverse((byte)((textMode - strlen(l2)) >> 1), menu_line + 1, l2, true);

    articles_bar();
    gotoxy(textMode - 1, menu_line + 1);


    switch (waitkey(false))
    {
    case BREAK:
        return ARTICLES_EXIT;
    case ARROW_UP:
        return ARTICLES_UP;
    case ARROW_DOWN:
        return ARTICLES_DOWN;
    case ENTER:
        return ARTICLES_SELECT;
    case ARROW_LEFT:
        return ARTICLES_PREV_PAGE;
    case ARROW_RIGHT:
        return ARTICLES_NEXT_PAGE;
    case 'G':
    case 'g':
        return ARTICLES_GOTO_PAGE;
    }

    return ARTICLES_MENU;
}

/**
 * @brief handle moving up, for current articles page
 */
ArticlesState articles_up(void)
{
    if (article_cursor_pos >  0)
    {
        articles_bar();
        article_cursor_pos--;
    }
    else
    {
        return ARTICLES_PREV_PAGE;
    }

    return ARTICLES_MENU;
}

/**
 * @brief handle moving down, for current articles page
 */
ArticlesState articles_down(void)
{
    if (article_cursor_pos<articles_on_this_page-1)
    {
        articles_bar();
        article_cursor_pos++;
    }
    else
    {
        return ARTICLES_NEXT_PAGE;
    }

    return ARTICLES_MENU;
}

/**
 * @brief Move to previous article page, if possible.
 */
ArticlesState articles_prev_page(void)
{
    if (articles_page>1)
        articles_page--;

    article_cursor_pos=article_cursor_pos_prev=(char) articles_per_page-1;

    memset(_articles, 0, MAX_ARTICLES_PER_PAGE * sizeof(struct Article));

    return ARTICLES_FETCH;
}

/**
 * @brief Move to next article page, if possible.
 */
ArticlesState articles_next_page(void)
{
    articles_page++;

    article_cursor_pos=article_cursor_pos_prev=0;

    memset(_articles, 0, MAX_ARTICLES_PER_PAGE * sizeof(struct Article));

    return ARTICLES_FETCH;
}

/**
 * @brief Move to a specific page.
 */
ArticlesState articles_goto_page(void)
{
    char inbuf[12];
    char *slash;
    int num = 0;
    int maxpage = 1;

    slash = strchr(page, '/');
    if (slash)
        maxpage = atoi(slash + 1);

    while (num < 1 || num > maxpage)
    {
        clear_screen(1);
        printf("Go to page (1-%d) or -1 to quit: ", maxpage);

        get_line(inbuf, sizeof(inbuf));

        if (inbuf[0] && inbuf[strlen(inbuf) - 1] == '\n')
        {
            inbuf[strlen(inbuf) - 1] = '\0';
        }

        if (!inbuf[0] || !strcmp(inbuf, "-1"))  // esc, too
        {
            num = -1;
            break;

        }
        else if (is_numeric(inbuf) == TRUE)
        {
            num = atoi(inbuf);
        }
        else
        {
            num = 0;
        }

        if (num < 1 || num > maxpage)
        {
            term_bell();
        }
    }

    if (num != -1)
    {
        articles_page = num;
    }

    article_cursor_pos=article_cursor_pos_prev=0;

    memset(_articles, 0, MAX_ARTICLES_PER_PAGE * sizeof(struct Article));

    return ARTICLES_FETCH;
}

/**
 * @brief handle Articles state
 */
State articles(void)
{
    articlesState=ARTICLES_RESET;

    while (state == ARTICLES)
    {
        switch(articlesState)
        {
        case ARTICLES_RESET:
            articlesState = articles_reset();
            break;
        case ARTICLES_FETCH:
            articlesState = articles_fetch();
            break;
        case ARTICLES_DISPLAY:
            articlesState = articles_display();
            break;
        case ARTICLES_MENU:
            articlesState = articles_menu();
            break;
        case ARTICLES_UP:
            articlesState = articles_up();
            break;
        case ARTICLES_DOWN:
            articlesState = articles_down();
            break;
        case ARTICLES_SELECT:
            article_id = _articles[article_cursor_pos].id;
            return ARTICLE;
        case ARTICLES_EXIT:
            return TOPICS;
        case ARTICLES_PREV_PAGE:
            articlesState = articles_prev_page();
            break;
        case ARTICLES_NEXT_PAGE:
            articlesState = articles_next_page();
            break;
        case ARTICLES_GOTO_PAGE:
            articlesState = articles_goto_page();
            break;
        }
    }

    return TOPICS;
}
