#include <fujinet-fuji.h>
#include <string.h>
#include <conio.h>
#include "stocks.h"
#include "init.h"
#include <screen.h>

AdapterConfigExtended ace;

/**
 * @brief Application entry point.
 *
 * Initialises the platform, runs the main loop until the user quits,
 * persists the stock list, and performs platform cleanup before exit.
 *
 * @return Always 0.
 */
int main(void)
{
    init();

    /* Hand off to the main application loop (screen.c).
       main_loop() returns only when the user presses BREAK. */
    main_loop();

    save_stocks();
    cleanup();

    return 0;
}
