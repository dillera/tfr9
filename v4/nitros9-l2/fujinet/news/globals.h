/**
 * @brief   News Reader
 * @author  Thomas Cherryhomes
 * @email   thom dot cherryhomes at gmail dot com
 * @license gpl v. 3, see LICENSE for details.
 * @verbose Global State variables
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include "screen.h"

#define ARROW_UP KEY_UP
#define ARROW_DOWN KEY_DOWN
#define ARROW_LEFT KEY_LEFT
#define ARROW_RIGHT KEY_RIGHT
#define ENTER KEY_ENTER
#define BREAK KEY_BREAK

#undef isdigit
#define isdigit(c) (c>=0x30 && c<=0x39)

/**
 * @brief available program states
 * @enum TOPICS current topics
 * @enum ARTICLES articles for given topic
 * @enum ARTICLE The currently selected article
 * @enum BYE exiting program
 * @enum HALT program halted.
 */
typedef enum _state
{
    TOPICS = 0,
    ARTICLES,
    ARTICLE,
    BYE,
} State;

typedef enum _selected_topic
{
    TOP_STORIES = 0,
    WORLD_NEWS,
    BUSINESS,
    SCIENCE,
    TECHNOLOGY,
    HEALTH,
    ENTERTAINMENT,
    POLITICS,
    SPORTS
} Topic;

/**
 * @brief currently selected topic
*/
extern Topic selectedTopic;

/**
 * @brief the current program state
 */
extern State state;

/**
 * @brief Current articles page
 */
extern int articles_page;

/**
 * @brief Number of articles per page
 */
extern int articles_per_page;

/**
 * @brief the currently selected article ID
 */
extern long article_id;

/**
 * @brief current article page
 */
extern int article_page;

/**
 * @brief Buffer for fetching status display
 */
extern char fetching_buf[];

/**
 * @brief Number of rows of text to fetch
 */
extern int rows;

/**
 * @brief Currently selected topic
 */
extern const char *topic;

/**
 * @brief The text mode: 80 columns, the tether's terminal
 */
extern unsigned char textMode;

/**
 * @brief Strings representing the available topics
 */
extern const char *topicStrings[];

/**
 * @brief Determine if string is numeric
 * @param s The string to test
 * @return true if the string is numeric, false otherwise
 */
BOOL is_numeric(const char *s);

#endif /* GLOBALS_H */
