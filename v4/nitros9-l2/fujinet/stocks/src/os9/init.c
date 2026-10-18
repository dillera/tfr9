#include <conio.h>
#include "init.h"

/**
 * @brief NitrOS-9 platform initialization.
 *
 * Takes the terminal: raw keys, no cursor, a clear screen.
 */
void init(void)
{
    term_init();
}

/**
 * @brief NitrOS-9 platform cleanup.
 *
 * Gives the terminal back, cleared, with its cursor.
 */
void cleanup(void)
{
    term_done();
}
