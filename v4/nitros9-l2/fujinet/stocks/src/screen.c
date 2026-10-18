/* -----------------------------------------------------------------------
 * screen.c
 * Main program loop, screen rendering, and user-input handling.
 *
 * Screen layout (42 x 24):
 *   Row  0     : scrolling stock ticker tape
 *   Rows 1-21  : main content (stock list, info screen, etc.)
 *   Rows 22-23 : static command menu
 * ----------------------------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <get_line.h>
#include <screen.h>
#include <network.h>
#include "stocks.h"


#ifndef GIT_VERSION
#  define GIT_VERSION "unknown"
#endif

/* NitrOS-9: colors on the tether's terminal (term.h); elsewhere, none. */
#if defined(BUILD_OS9)
#  define COLOR(fg, bg)  term_color((fg), (bg))
#else
#  define COLOR(fg, bg)
#endif

/** @brief Scroll buffer and state for the ticker tape at row 0. */
#define SCROLL_BUF_LEN  384
static char          scroll_buf[SCROLL_BUF_LEN];
static unsigned int  scroll_len  = 0;
static unsigned int  scroll_pos  = 0;
static uint8_t frame_count = 0;
#if defined(BUILD_OS9)
static uint8_t       scroll_col[SCROLL_BUF_LEN];   /* each character's color */
#endif

/* Shared symbol/scratch buffer.  draw_slot, draw_lookup_row, edit_stock,
 * lookup_screen, and show_stock_info are all mutually exclusive (each is
 * either modal or exclusive to a particular screen state), so one buffer
 * serves all.  Size 16 accommodates the longest use (phone_str). */
static char sym_buf[16];

/* -----------------------------------------------------------------------
 * Screen primitives
 * ----------------------------------------------------------------------- */

/**
 * @brief Write n space characters to stdout.
 *
 * @param n Number of spaces to write.
 */
static void fill_spaces(uint8_t n)
{
    while (n-- > 0)
        putchar(' ');
}

/**
 * @brief Print a string centred within SCREEN_WIDTH on the current row.
 *
 * Leading spaces centre the string; no trailing padding is added.
 *
 * @param s The string to centre.
 */
void draw_centered(const char *s)
{
    fill_spaces((uint8_t)((SCREEN_WIDTH - strlen(s)) / 2));
#if defined(BUILD_OS9)
    /* The keys, <X>, black on cyan, in a line of cyan; anything else in
     * the color set before. */
    if (strchr(s, '<')) {
        for (; *s; s++) {
            if (*s == '<')
                COLOR(TC_BLACK, TC_CYAN);
            putchar(*s);
            if (*s == '>')
                COLOR(TC_CYAN, TC_DEFAULT);
        }
        COLOR(TC_DEFAULT, TC_DEFAULT);
        return;
    }
#endif
    printf("%s", s);
}

/**
 * @brief Clear a single screen row.
 *
 * @param row The row number to clear.
 */
void clear_line(uint8_t row)
{
    /* On cc65 targets (Atari, Apple II) use cclearxy() which writes directly
     * to screen RAM without going through the OS text device, avoiding the
     * logical-line cursor-wrap issue that shifts subsequent gotoxy() targets.
     * All other platforms can safely use putchar via fill_spaces. */
#if defined(BUILD_APPLE2) || defined(BUILD_ATARI)
    cclearxy(0, (uint8_t)row, SCREEN_WIDTH);
    gotoxy(0, row);
#else
    gotoxy(0, row);
    fill_spaces(SCREEN_WIDTH);
    gotoxy(0, row);
#endif
}

/**
 * @brief Blank rows 1-19; rows 20-21 are left for draw_app_name() to overwrite.
 */
static void clear_content_area(void)
{
    uint8_t row;
    for (row = CONTENT_TOP_ROW; row < MENU_ROW1 - 2; row++) {
        gotoxy(0, row);
        clear_line(row);
    }
}

/* -----------------------------------------------------------------------
 * Progress display
 * ----------------------------------------------------------------------- */

/** @brief Progress message displayed on the ticker row during network calls. */
static char progress_msg[SCREEN_WIDTH + 1];

/**
 * @brief Set the progress message and display it on the ticker row.
 *
 * @param msg Message to display.
 */
void set_progress_message(const char *msg)
{
    strncpy(progress_msg, msg, sizeof(progress_msg) - 1);
    progress_msg[sizeof(progress_msg) - 1] = '\0';
    gotoxy(0, TICKER_ROW);
    COLOR(TC_BRIGHT + TC_YELLOW, TC_DEFAULT);
    printf("%s", progress_msg);
    COLOR(TC_DEFAULT, TC_DEFAULT);
}

/**
 * @brief Append a dot to the progress message and redisplay it on the ticker row.
 */
void update_progress_message(void)
{
    uint8_t len = (uint8_t)strlen(progress_msg);
    if (len < sizeof(progress_msg) - 1) {
        progress_msg[len]     = '.';
        progress_msg[len + 1] = '\0';
    }
    gotoxy(0, TICKER_ROW);
    COLOR(TC_BRIGHT + TC_YELLOW, TC_DEFAULT);
    printf("%s", progress_msg);
    COLOR(TC_DEFAULT, TC_DEFAULT);
}

/* -----------------------------------------------------------------------
 * App chrome
 * ----------------------------------------------------------------------- */

/**
 * @brief Print the application title and optionally a "Please Wait" banner.
 *
 * @param show_wait    If true, prints "*** Please Wait ***" below the title.
 * @param show_version If true (and show_wait is false), prints the version string below the title.
 */
void draw_app_name(bool show_wait, bool show_version)
{
    gotoxy(0, MENU_ROW1 - 2);
    COLOR(TC_BRIGHT + TC_YELLOW, TC_DEFAULT);
    draw_centered("*** FujiNet Stocks ***");

    COLOR(TC_DEFAULT, TC_DEFAULT);
    clear_line(MENU_ROW1 - 1);
    if (show_wait) {
        gotoxy(0, MENU_ROW1 - 1);
        COLOR(TC_BRIGHT + TC_GREEN, TC_DEFAULT);
        draw_centered("*** Please Wait ***");
    } else if (show_version) {
        gotoxy(0, MENU_ROW1 - 1);
        COLOR(TC_CYAN, TC_DEFAULT);
        draw_centered("Version: " GIT_VERSION);
    }
    COLOR(TC_DEFAULT, TC_DEFAULT);
}

/**
 * @brief Render the two-line command reference at rows 22-23.
 *
 * @param show_stocks true when the stock list is visible (ignored when lookup).
 * @param lookup      true to show the lookup screen menu instead of the main menu.
 */
static void draw_menu(bool show_stocks, bool lookup)
{
    draw_app_name(false, !lookup);
    clear_line(MENU_ROW1);
    clear_line(MENU_ROW2);

    gotoxy(0, MENU_ROW1);
    COLOR(TC_CYAN, TC_DEFAULT);
    if (lookup)
        draw_centered("Arrows:Select  <ENTER>:Choose");
    else if (show_stocks)
        draw_centered("<H>ide <E>dit <D>el <I>nfo <L>ookup");
    else
        draw_centered("<S>how <L>ookup");

    gotoxy(0, MENU_ROW2);
    COLOR(TC_CYAN, TC_DEFAULT);
    if (lookup)
        draw_centered("<" BREAK_KEY_NAME ">:Return");
    else if (show_stocks)
        draw_centered("0-9/Arrows:Sel <R>efresh <" BREAK_KEY_NAME ">:Quit");
    else
        draw_centered("<R>efresh <" BREAK_KEY_NAME ">:Quit");
}

/* -----------------------------------------------------------------------
 * Ticker
 * ----------------------------------------------------------------------- */

/**
 * @brief Write a SCREEN_WIDTH-character window from scroll_buf into row 0.
 */
static void draw_ticker_row(void)
{
    unsigned int idx = scroll_pos;
    uint8_t      col;
    char         c;

    gotoxy(0, TICKER_ROW);
    for (col = 0; col < SCREEN_WIDTH - 1; col++) {
        c = scroll_buf[idx];
        if ((uint8_t)c < 0x20)
            c = ' ';
        COLOR(scroll_col[idx], TC_BLUE);
        putchar(c);
        if (++idx >= scroll_len)
            idx = 0;
    }
    COLOR(scroll_col[idx], TC_BLUE);
    /* Last cell via cputcxy: avoids cursor-advance at row end on cc65 targets.
     * On CoCo/MSDOS, cputcxy is defined as gotoxy+putchar. */
    c = scroll_buf[idx];
    if ((uint8_t)c < 0x20)
        c = ' ';
    cputcxy((uint8_t)(SCREEN_WIDTH - 1), TICKER_ROW, c);
    COLOR(TC_DEFAULT, TC_DEFAULT);
}

#if defined(BUILD_OS9)
/**
 * @brief Color the ticker tape: "SYMBOL $PRICE +PCT%", symbol bright
 * white, price yellow, the change green if up and red if down.
 */
static void color_ticker(void)
{
    unsigned int i;
    uint8_t      token = 0, spaces = 0, color = TC_WHITE;

    for (i = 0; i < scroll_len; i++) {
        if (scroll_buf[i] == ' ') {
            spaces++;
            scroll_col[i] = TC_WHITE;
            continue;
        }
        if (spaces || i == 0) {
            /* A new word: more than one space before it starts a stock. */
            token = (spaces > 1 || i == 0) ? 0 : token + 1;
            spaces = 0;
            if (token == 0)
                color = TC_BRIGHT + TC_WHITE;
            else if (token == 1)
                color = TC_BRIGHT + TC_YELLOW;
            else
                color = scroll_buf[i] == '-' ? TC_BRIGHT + TC_RED
                      : scroll_buf[i] == '+' ? TC_BRIGHT + TC_GREEN : TC_WHITE;
        }
        scroll_col[i] = color;
    }
}
#endif

/* -----------------------------------------------------------------------
 * Stock list
 * ----------------------------------------------------------------------- */

/**
 * @brief Render one stock slot at its pre-computed screen position.
 *
 * The "%2d: " prefix is always normal video; only the 10-character symbol
 * field is shown in inverse video when highlighted.  An empty slot shows
 * 10 spaces in the symbol field.
 *
 * @param i           Slot index to render.
 * @param highlighted true to render the symbol field in inverse video.
 */
static void draw_slot(uint8_t i, bool highlighted)
{
    if (stocks[i].symbol[0] != '\0') {
        snprintf(sym_buf, 11, "%-10s", stocks[i].symbol);
    } else {
        memset(sym_buf, ' ', 10);
        sym_buf[10] = '\0';
    }

    gotoxy(stock_coords[i].x, stock_coords[i].y);
    COLOR(TC_CYAN, TC_DEFAULT);
    printf("%2d: ", i + 1);
    COLOR(TC_BRIGHT + TC_WHITE, TC_DEFAULT);
    revers(highlighted);
    cputs(sym_buf);
    revers(0);
    COLOR(TC_DEFAULT, TC_DEFAULT);
}

/**
 * @brief Render all stock slots; called on full redraws.
 *
 * @param selected Index of the currently selected slot.
 */
static void draw_stock_list(uint8_t selected)
{
    uint8_t i;
    for (i = 0; i < MAX_STOCKS; i++)
        draw_slot(i, i == selected);
}

/* -----------------------------------------------------------------------
 * Lookup UI
 * ----------------------------------------------------------------------- */

/**
 * @brief Render one row from the global lookup_results.
 *
 * @param i           Result index to render.
 * @param highlighted true to render the row in inverse video.
 */
static void draw_lookup_row(uint8_t i, bool highlighted)
{
    static char line[41];
    static char desc[28];

    strncpy(sym_buf, lookup_results.results[i].displaySymbol, 12); sym_buf[12] = '\0';
    strncpy(desc,    lookup_results.results[i].description,   27); desc[27]    = '\0';
    snprintf(line, sizeof(line), "%-12s %-27s", sym_buf, desc);
    line[40] = '\0';
#if defined(BUILD_APPLE2)
    /* Apple II inverse video only supports uppercase; lowercase renders as
     * garbled symbols.  Uppercase the line before printing in inverse. */
    if (highlighted) strupr(line);
#endif
    gotoxy(0, 4 + i);
    COLOR(TC_BRIGHT + TC_WHITE, TC_DEFAULT);
    revers(highlighted);
    cputs(line);
    revers(0);
    COLOR(TC_DEFAULT, TC_DEFAULT);
}

/* -----------------------------------------------------------------------
 * Formatting utilities  (used by the stock info screen)
 * ----------------------------------------------------------------------- */

/**
 * @brief Format a value in millions as a human-readable string with T/B/M suffix.
 *
 * Uses integer arithmetic only (no floats).  Examples:
 *   3878463 -> "3.87T",  250000 -> "250.00B",  500 -> "500.00M"
 *
 * @param millions Value in millions to format.
 * @param out      Output buffer.
 * @param out_len  Size of the output buffer.
 */
static void format_large_num(long millions, char *out, uint8_t out_len)
{
    long whole, frac;
    char suffix;

    if (millions >= 1000000L) {
        whole  = millions / 1000000L;
        frac   = (millions % 1000000L) / 10000L;
        suffix = 'T';
    } else if (millions >= 1000L) {
        whole  = millions / 1000L;
        frac   = (millions % 1000L) / 10L;
        suffix = 'B';
    } else {
        whole  = millions;
        frac   = 0;
        suffix = 'M';
    }
    snprintf(out, (size_t)out_len, "%ld.%02ld%c", whole, frac, suffix);
}

/**
 * @brief Format a raw phone digit string as a dashed phone number.
 *
 * Strips non-digit characters (e.g. trailing ".0" from the API), then:
 *   11 digits starting with 1 -> "1-XXX-XXX-XXXX"
 *   10 digits              -> "XXX-XXX-XXXX"
 *   anything else          -> raw string as-is
 *
 * @param raw     Input phone string from the API.
 * @param out     Output buffer.
 * @param out_len Size of the output buffer.
 */
static void format_phone(const char *raw, char *out, uint8_t out_len)
{
    char          digits[16];
    uint8_t n = 0;
    const char   *p;

    for (p = raw; *p && n < sizeof(digits) - 1; p++) {
        if (*p >= '0' && *p <= '9')
            digits[n++] = *p;
    }
    digits[n] = '\0';

    if (n == 11 && digits[0] == '1')
    {
        snprintf(out, (size_t)out_len, "%c-%c%c%c-%c%c%c-%c%c%c%c",
                 digits[0],
                 digits[1], digits[2], digits[3],
                 digits[4], digits[5], digits[6],
                 digits[7], digits[8], digits[9], digits[10]);
    }
    else if (n == 10)
    {
        snprintf(out, (size_t)out_len, "%c%c%c-%c%c%c-%c%c%c%c",
                 digits[0], digits[1], digits[2],
                 digits[3], digits[4], digits[5],
                 digits[6], digits[7], digits[8], digits[9]);
    }
    else
    {
        strncpy(out, raw, (size_t)(out_len - 1));
    }
    out[out_len - 1] = '\0';
}

/* -----------------------------------------------------------------------
 * Modal screens
 * ----------------------------------------------------------------------- */

/**
 * @brief Print a label and its value on the info screen.
 *
 * @param row   Row to print on, or 255 to go on from the cursor.
 * @param label The label, e.g. "Symbol  : ".
 * @param value The value.
 */
static void show_field(uint8_t row, const char *label, const char *value)
{
    if (row != 255)
        gotoxy(0, row);
    COLOR(TC_CYAN, TC_DEFAULT);
    printf("%s", label);
    COLOR(TC_BRIGHT + TC_WHITE, TC_DEFAULT);
    printf("%s", value);
    COLOR(TC_DEFAULT, TC_DEFAULT);
}

/**
 * @brief Display the detail info screen for the selected stock.
 *
 * Uses global stock_info (filled by get_stock_info()).
 *
 * @param selected Slot index of the stock to display.
 */
static void show_stock_info(uint8_t selected)
{
    if (stocks[selected].symbol[0] == '\0')
        return;

    clrscr();
    draw_app_name(true, true);
    get_stock_info(stocks[selected].symbol);
    clear_line(MENU_ROW1 - 1);
    clear_line(TICKER_ROW);
    gotoxy(0, TICKER_ROW);
    COLOR(TC_BRIGHT + TC_YELLOW, TC_DEFAULT);
    draw_centered("*** Stock Information ***");
    COLOR(TC_DEFAULT, TC_DEFAULT);

    show_field(5, "Symbol  : ", stock_info.symbol);
    show_field(6, "Company : ", stock_info.name);
    show_field(7, "Exchange: ", stock_info.exchange);
    show_field(8, "Country : ", stock_info.country);
    show_field(255, "  Currency: ", stock_info.currency);
    show_field(9, "Industry: ", stock_info.industry);
    show_field(10, "IPO     : ", stock_info.ipo);

    format_large_num(stock_info.market_cap, sym_buf, sizeof(sym_buf));
    show_field(11, "Mkt Cap : ", sym_buf);

    format_large_num(stock_info.shares_outstanding, sym_buf, sizeof(sym_buf));
    show_field(12, "Shares  : ", sym_buf);

    format_phone(stock_info.phone, sym_buf, sizeof(sym_buf));
    show_field(13, "Phone   : ", sym_buf);

    show_field(14, "Web     : ", stock_info.weburl);

    clear_line(MENU_ROW1);
    gotoxy(0, MENU_ROW1);
    draw_centered("* Press any key to return *");

    clear_line(MENU_ROW2);

#if defined(_CMOC_VERSION_) && !defined(BUILD_OS9)
    while (!inkey())
        ;
#else
    while (!kbhit())
        ;
    cgetc();
#endif
}

/**
 * @brief Edit the symbol in the selected slot in-place.
 *
 * Positions at the slot and lets get_line() accept input right in the
 * symbol field.
 *
 * @param selected Slot index to edit.
 * @return true if the user committed a new symbol, false if aborted (BREAK).
 */
static bool edit_stock(uint8_t selected)
{
    sym_buf[0] = '\0';
    draw_slot(selected, false);
    gotoxy(stock_coords[selected].x, stock_coords[selected].y);
    printf("%2d: ", selected + 1);

    get_line(sym_buf, SYMBOL_LEN);
    if (sym_buf[0] != '\0') {
        strupr(sym_buf);
        strncpy(stocks[selected].symbol, sym_buf, SYMBOL_LEN);
        stocks[selected].price = 0;
        stocks[selected].change = 0;
        stocks[selected].change_pct = 0;
        return true;
    }
    return false;
}

/**
 * @brief Prompt for a search string, display results, and let the user choose.
 *
 * Calls lookup_stock_ticker() which fills global lookup_results, then
 * displays them and allows the user to select one.
 *
 * @param return_slot   Slot index to store the chosen symbol into.
 * @param update_stocks If false, ENTER returns without modifying the stock list.
 */
static bool lookup_screen(uint8_t return_slot, bool update_stocks)
{
    uint8_t lk_sel = 0, i;
    char key;

    sym_buf[0] = '\0';
    clrscr();
    draw_app_name(false, false);
    gotoxy(0, TICKER_ROW);
    printf("Enter search term - <" BREAK_KEY_NAME "> to cancel:");
    gotoxy(0, TICKER_ROW + 1);
    printf("> ");
    get_line(sym_buf, SYMBOL_LEN);
    clear_line(TICKER_ROW);
    clear_line(TICKER_ROW + 1);
    if (sym_buf[0] == '\0')
        return false;

    draw_app_name(true, false);
    set_progress_message("Searching");
    lookup_stock_ticker(sym_buf);   /* fills global lookup_results */

    clear_line(TICKER_ROW);
    clear_content_area();

    if (lookup_results.count == 0) {
        gotoxy(0, 2);
        printf("No results found.  Press any key.");
#if defined(_CMOC_VERSION_) && !defined(BUILD_OS9)
        while (!inkey())
            ;
#else
        while (!kbhit())
            ;
        cgetc();    /* or the main loop takes the key as a command */
#endif
        return false;
    }

    show_field(2, "Results for: ", sym_buf);

    for (i = 0; i < lookup_results.count; i++)
        draw_lookup_row(i, i == lk_sel);

    draw_menu(false, true);
    clear_line(MENU_ROW1 - 1);

    for (;;) {
#if defined(_CMOC_VERSION_) && !defined(BUILD_OS9)
        while (!(key = inkey()))
            ;
#else
        while (!kbhit())
            ;
        key = cgetc();
#endif

        if (key == BREAK)
            return false;

        if (key == ENTER) {
            if (!update_stocks)
                return false;
            memset(&stocks[return_slot], 0, sizeof(Stock));
            strncpy(stocks[return_slot].symbol,
                    lookup_results.results[lk_sel].displaySymbol,
                    SYMBOL_LEN - 1);
            return true;
        }

        if (key == ARROW_DOWN && lk_sel < lookup_results.count - 1) {
            draw_lookup_row(lk_sel, false);
            draw_lookup_row(++lk_sel, true);
        }

        if (key == ARROW_UP && lk_sel > 0) {
            draw_lookup_row(lk_sel, false);
            draw_lookup_row(--lk_sel, true);
        }
    }
}

/* -----------------------------------------------------------------------
 * Input / navigation
 * ----------------------------------------------------------------------- */

/**
 * @brief Return the new selection index after a directional key press.
 *
 * Grid is two columns of five rows:
 *   col 0 (left):  slots 0-4
 *   col 1 (right): slots 5-9
 *
 * @param current Current slot index.
 * @param key     Arrow key character.
 * @return New slot index (unchanged if the move would leave the grid).
 */
static uint8_t move_selection(uint8_t current, char key)
{
    switch (key) {
        case ARROW_RIGHT:
            if (current < 5)
                return current + 5;
            break;
        case ARROW_LEFT:
            if (current >= 5)
                return current - 5;
            break;
        case ARROW_DOWN:
            if (current % 5 < 4)
                return current + 1;
            break;
        case ARROW_UP:
            if (current % 5 > 0)
                return current - 1;
            break;
        default:
            break;
    }
    return current;
}

/* -----------------------------------------------------------------------
 * Main loop
 * ----------------------------------------------------------------------- */

/**
 * @brief Main application loop; returns when the user presses BREAK.
 *
 * Arrow keys only redraw the two affected slots rather than the full list.
 * All other operations that change data use need_redraw.
 */
void main_loop(void)
{
    bool          show_stocks         = true;
    uint8_t selected            = 0;
    uint8_t new_sel;
    char    key;
    bool    need_redraw         = true;
    bool    need_scroll_rebuild = true;
    clock_t last_quote_time     = clock();

    clrscr();
    draw_app_name(true, true);
    load_stocks();
    last_quote_time = get_stock_quotes();
    clear_line(TICKER_ROW);

    for (;;) {

        if (clock() - last_quote_time >= GET_QUOTES_INTERVAL) {
            clear_line(TICKER_ROW);
            last_quote_time     = get_stock_quotes();
            need_scroll_rebuild = true;
            need_redraw         = true;
        }

        if (need_redraw) {
            clear_content_area();
            if (show_stocks)
                draw_stock_list(selected);
            draw_menu(show_stocks, false);
            need_redraw = false;
        }

        if (need_scroll_rebuild) {
            build_scroll_string(scroll_buf, SCROLL_BUF_LEN);
            scroll_len          = (unsigned int)strlen(scroll_buf);
#if defined(BUILD_OS9)
            color_ticker();
#endif
            scroll_pos          = 0;
            need_scroll_rebuild = false;
        }

        sync_frame();

        /* Only when it moves: on a terminal every row drawn is sent. */
        if (++frame_count >= SCROLL_FRAMES) {
            frame_count = 0;
            if (++scroll_pos >= scroll_len)
                scroll_pos = 0;
            draw_ticker_row();
        }

#if defined(_CMOC_VERSION_) && !defined(BUILD_OS9)
        key = inkey();
        if (!key)
            continue;
#else
        if (!kbhit())
            continue;
        key = cgetc();
#endif

        if (key == BREAK)
            break;

        switch (key) {

            case 'S': case 's':
                if (!show_stocks) {
                    show_stocks = true;
                    need_redraw = true;
                }
                break;

            case 'H': case 'h':
                if (show_stocks) {
                    draw_slot(selected, false);
                    show_stocks = false;
                    need_redraw = true;
                }
                break;

            case ARROW_UP:
            case ARROW_DOWN:
            case ARROW_LEFT:
            case ARROW_RIGHT:
                if (show_stocks) {
                    new_sel = move_selection(selected, key);
                    if (new_sel != selected) {
                        draw_slot(selected, false);
                        draw_slot(new_sel,  true);
                        selected = new_sel;
                    }
                }
                break;

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                if (show_stocks) {
                    new_sel = (key == '0') ? 9 : (key - '1');
                    if (new_sel != selected) {
                        draw_slot(selected, false);
                        draw_slot(new_sel,  true);
                        selected = new_sel;
                    }
                }
                break;

            case 'E': case 'e':
                if (show_stocks) {
                    if (edit_stock(selected)) {
                        sort_stocks();
                        save_stocks();
                        clear_line(TICKER_ROW);
                        last_quote_time     = get_stock_quotes();
                        selected            = 0;
                        need_scroll_rebuild = true;
                    }
                    need_redraw = true;
                }
                break;

            case 'R': case 'r':
                clear_line(TICKER_ROW);
                draw_app_name(true, true);
                last_quote_time     = get_stock_quotes();
                draw_app_name(false, true);
                need_scroll_rebuild = true;
                need_redraw         = true;
                break;

            case 'D': case 'd':
                if (show_stocks && stocks[selected].symbol[0] != '\0') {
                    delete_stock(selected);
                    sort_stocks();
                    save_stocks();
                    if (selected >= MAX_STOCKS - 1)
                        selected = MAX_STOCKS - 2;
                    need_scroll_rebuild = true;
                    need_redraw         = true;
                }
                break;

            case 'I': case 'i':
                if (show_stocks && stocks[selected].symbol[0] != '\0') {
                    show_stock_info(selected);
                    clrscr();
                    draw_app_name(true, true);
                    need_redraw         = true;
                    need_scroll_rebuild = true;
                }
                break;

            case 'L': case 'l':
                if (lookup_screen(selected, show_stocks)) {
                    sort_stocks();
                    save_stocks();
                    clrscr();
                    draw_app_name(true, true);
                    last_quote_time = get_stock_quotes();
                }
                clrscr();
                draw_app_name(true, true);
                selected            = 0;
                need_redraw         = true;
                need_scroll_rebuild = true;
                break;

            default:
                break;
        }
    }
}
