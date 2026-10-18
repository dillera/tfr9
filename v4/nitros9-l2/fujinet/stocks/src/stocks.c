/* -----------------------------------------------------------------------
 * stocks.c
 * Stock data management, formatting utilities, and persistence.
 * ----------------------------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <screen.h>
#include "stocks.h"
#include "appkey.h"
#include "network.h"

Stock stocks[MAX_STOCKS];

/* Shared scratch buffer.  load/save/parse run at startup/exit only;
 * build_scroll_string runs in the main loop — they never overlap. */
static char stock_io_buf[64];

/**
 * @brief Pre-computed screen positions for each stock slot.
 *
 * X positions derived from SLOT_LEFT_X / SLOT_RIGHT_X (screen.h), which are
 * computed from SCREEN_WIDTH so the layout centres correctly on both
 * 42-column (CoCo) and 40-column targets.
 *
 * Slots 0-4 fill the left column top-to-bottom; slots 5-9 fill the right
 * column top-to-bottom.  Rows 5, 8, 11, 14, 17 (2 blank rows between each).
 * Vertical padding: 4 blank rows above (rows 1-4) and below (rows 18-21).
 */
const Coord stock_coords[MAX_STOCKS] = {
    { SLOT_LEFT_X,   5 },   /* slot  0  (stock  1) */
    { SLOT_LEFT_X,   8 },   /* slot  1  (stock  2) */
    { SLOT_LEFT_X,  11 },   /* slot  2  (stock  3) */
    { SLOT_LEFT_X,  14 },   /* slot  3  (stock  4) */
    { SLOT_LEFT_X,  17 },   /* slot  4  (stock  5) */
    { SLOT_RIGHT_X,  5 },   /* slot  5  (stock  6) */
    { SLOT_RIGHT_X,  8 },   /* slot  6  (stock  7) */
    { SLOT_RIGHT_X, 11 },   /* slot  7  (stock  8) */
    { SLOT_RIGHT_X, 14 },   /* slot  8  (stock  9) */
    { SLOT_RIGHT_X, 17 }    /* slot  9  (stock 10) */
};

/* -----------------------------------------------------------------------
 * Formatting / display
 * ----------------------------------------------------------------------- */

/**
 * @brief Convert a price in integer cents to a display string.
 *
 * Examples: 1234 -> "12.34", 100 -> "1.00", 50 -> "0.50"
 *
 * @param cents   Value scaled by 100 (e.g. 1234 represents $12.34).
 * @param out     Output buffer; must be at least out_len bytes.
 * @param out_len Size of the output buffer.
 */
void format_price(long cents, char *out, uint8_t out_len)
{
    long dollars;
    uint8_t rem, neg, pos, dstart, len, i;
    char t;

    neg = (cents < 0);
    if (neg) cents = -cents;
    dollars = cents / 100L;
    rem     = (uint8_t)(cents % 100L);

    pos = 0;
    if (neg) out[pos++] = '-';
    dstart = pos;

    /* build dollar digits in reverse, then flip in place */
    do {
        out[pos++] = (char)('0' + (int)(dollars % 10L));
        dollars /= 10L;
    } while (dollars > 0L);
    len = pos - dstart;
    for (i = 0; i < len / 2; i++) {
        t = out[dstart + i];
        out[dstart + i]           = out[dstart + len - 1 - i];
        out[dstart + len - 1 - i] = t;
    }

    out[pos++] = '.';
    out[pos++] = (char)('0' + rem / 10);
    out[pos++] = (char)('0' + rem % 10);
    out[pos]   = '\0';
    (void)out_len;
}

/**
 * @brief Build the ticker-tape string that scrolls across row 0.
 *
 * Format per stock: "SYMBOL $PP.PP +/-CC.CC%    ". Only populated slots are
 * included.  The buffer is a circular tape; draw_ticker_row() reads
 * SCREEN_WIDTH chars at scroll_pos with modulo wrap.  Padded with spaces
 * when shorter than SCREEN_WIDTH so the window never repeats.
 *
 * @param buf     Output buffer for the scroll string.
 * @param buf_len Size of the output buffer.
 */
void build_scroll_string(char *buf, int buf_len)
{
    uint8_t i, j, m;
    int pos = 0;
    static char price_str[12];
    static char change_str[12];
    static char pct_str[14];   /* sign(1) + digits(up to 10) + '%'(1) + NUL */
    long abs_change;

    for (i = 0; i < MAX_STOCKS; i++) {
        if (stocks[i].symbol[0] == '\0')
            continue;

        format_price(stocks[i].price, price_str, sizeof(price_str));

        abs_change = stocks[i].change_pct < 0 ? -(long)stocks[i].change_pct
                                              :  (long)stocks[i].change_pct;
        format_price(abs_change, change_str, sizeof(change_str));

        /* Build pct_str = sign + digits + "%" — avoids %c in snprintf which
         * misaligns the vararg stack on Open Watcom 16-bit.              */
        pct_str[0] = stocks[i].change_pct > 0 ? '+' :
                     stocks[i].change_pct < 0 ? '-' : ' ';
        for (m = 0; change_str[m] && m < (int)sizeof(pct_str) - 3; m++)
            pct_str[m + 1] = change_str[m];
        pct_str[m + 1] = '%';
        pct_str[m + 2] = '\0';

        snprintf(stock_io_buf, sizeof(stock_io_buf), "%s $%s %s    ",
                 stocks[i].symbol, price_str, pct_str);

        for (j = 0; stock_io_buf[j] != '\0' && pos < buf_len - 1; j++)
            buf[pos++] = stock_io_buf[j];
    }

    /* Pad to at least SCREEN_WIDTH so the circular window never repeats */
    while (pos < SCREEN_WIDTH && pos < buf_len - 1)
        buf[pos++] = ' ';

    buf[pos] = '\0';
}

/* -----------------------------------------------------------------------
 * Stock data management
 * ----------------------------------------------------------------------- */

/**
 * @brief Sort the stock list alphabetically, with empty slots at the end.
 *
 * Uses insertion sort (stable, n=10).  Non-empty slots are sorted by
 * symbol in ascending order; empty slots bubble to the tail.
 */
void sort_stocks(void)
{
    uint8_t i;
    signed char   j;
    static Stock  tmp;

    for (i = 1; i < MAX_STOCKS; i++) {
        tmp = stocks[i];
        j   = i - 1;
        while (j >= 0 && tmp.symbol[0] != '\0' &&
               (stocks[j].symbol[0] == '\0' ||
                strcmp(stocks[j].symbol, tmp.symbol) > 0)) {
            stocks[j + 1] = stocks[j];
            j--;
        }
        stocks[j + 1] = tmp;
    }
}

/**
 * @brief Remove the stock at index and collapse the list.
 *
 * @param index Slot index of the stock to remove.
 */
void delete_stock(uint8_t index)
{
    if (index >= MAX_STOCKS)
        return;

    memset(&stocks[index], 0, sizeof(Stock));
}

/**
 * @brief Refresh quote data for all populated stock slots.
 *
 * Iterates stocks[] and calls get_stock_quote() for every non-empty slot.
 */
clock_t get_stock_quotes(void)
{
    uint8_t i;
    set_progress_message("Updating stocks");
    for (i = 0; i < MAX_STOCKS; i++) {
        if (stocks[i].symbol[0] != '\0') {
            get_stock_quote(&stocks[i]);
            update_progress_message();
        }
    }
    return clock();
}

/* -----------------------------------------------------------------------
 * Persistence
 * ----------------------------------------------------------------------- */

/**
 * @brief Split the pipe-separated list in stock_io_buf into up to 5 stock slots.
 *
 * Reads from the global stock_io_buf scratch buffer.  Tokens beyond 5 are ignored.
 *
 * @param base_slot Starting slot index in the stocks[] array.
 */
static void parse_symbols_into_slots(uint8_t base_slot)
{
    char         *tok;
    uint8_t slot = base_slot;

    tok = strtok(stock_io_buf, "|");
    while (tok != NULL && slot < base_slot + 5) {
        strncpy(stocks[slot].symbol, tok, SYMBOL_LEN - 1);
        stocks[slot].symbol[SYMBOL_LEN - 1] = '\0';
        slot++;
        tok = strtok(NULL, "|");
    }
}

/**
 * @brief Build a pipe-separated symbol list from up to 5 stock slots.
 *
 * Empty slots are skipped.
 *
 * @param buf       Output buffer; must be at least 64 bytes.
 * @param base_slot Starting slot index in the stocks[] array.
 */
static void build_symbols_from_slots(char *buf, uint8_t base_slot)
{
    uint8_t i, pos = 0, first = 1;

    buf[0] = '\0';
    for (i = base_slot; i < base_slot + 5; i++) {
        if (stocks[i].symbol[0] == '\0')
            continue;
        if (!first)
            buf[pos++] = '|';
        strncpy(buf + pos, stocks[i].symbol, SYMBOL_LEN - 1);
        pos += (uint8_t)strlen(stocks[i].symbol);
        first = 0;
    }
    buf[pos] = '\0';
}

/**
 * @brief Read up to 10 stock symbols from two app keys and populate stocks[].
 *
 * Key 1 fills slots 0-4; key 2 fills slots 5-9.  Each key holds a
 * pipe-separated list, e.g. "AAPL|GOOG".
 */
void load_stocks(void)
{
    if (read_appkey(stock_io_buf, sizeof(stock_io_buf), STOCKS_APP_KEY_1))
        parse_symbols_into_slots(0);

    if (read_appkey(stock_io_buf, sizeof(stock_io_buf), STOCKS_APP_KEY_2))
        parse_symbols_into_slots(5);

    sort_stocks();
}

/**
 * @brief Persist the current stocks[] symbol list to two app keys.
 *
 * Key 1 holds slots 0-4; key 2 holds slots 5-9.  Symbols are written
 * as a pipe-separated list into a 64-byte buffer.
 */
void save_stocks(void)
{
    build_symbols_from_slots(stock_io_buf, 0);
    write_appkey(stock_io_buf, STOCKS_APP_KEY_1);

    build_symbols_from_slots(stock_io_buf, 5);
    write_appkey(stock_io_buf, STOCKS_APP_KEY_2);
}
