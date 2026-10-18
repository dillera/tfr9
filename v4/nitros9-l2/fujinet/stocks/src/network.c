/* -----------------------------------------------------------------------
 * network.c
 * FujiNet network calls for retrieving stock data.
 * ----------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <fujinet-network.h>
#include "network.h"
#include "init.h"
#include <screen.h>


/** @brief Global result buffers — too large for the 8-bit stack. */
LookupResults lookup_results;
StockInfo     stock_info;

/* Shared scratch buffers for network functions.  All network calls are
 * sequential in this single-threaded app, so one copy of each suffices. */
static char net_url[128];
static char net_val[64];
static char net_path[32];
static char net_encoded[SYMBOL_LEN * 3 + 1];

/* -----------------------------------------------------------------------
 * Utilities
 * ----------------------------------------------------------------------- */

/**
 * @brief Check a FujiNet return code and abort on failure.
 *
 * If err is not FN_ERR_OK, prints msg, runs platform cleanup, and exits
 * with a non-zero status.
 *
 * @param err FujiNet error code returned by a network call.
 * @param msg Human-readable description of the operation that failed.
 */
void check_error(uint8_t err, const char *msg)
{
    if (err != FN_ERR_OK) {
        clrscr();
        printf("%s", msg);
        gotoxy(0, MENU_ROW1);
        draw_centered("* Press any key to exit *");

    clear_line(MENU_ROW2);

    while (!kbhit())
        ;

        cleanup();
        exit(1);
    }
}

/**
 * @brief Percent-encode a string for use in a URL query parameter.
 *
 * Only unreserved characters (A-Z a-z 0-9 - _ . ~) are passed through;
 * everything else becomes %XX.
 *
 * @param src     Input string to encode.
 * @param out     Output buffer; must be at least 3*strlen(src)+1 bytes.
 * @param out_len Size of the output buffer.
 */
void url_encode(const char *src, char *out, int out_len)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t pos = 0, c;

    while (*src && pos < out_len - 3) {
        c = (uint8_t)*src++;
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out[pos++] = (char)c;
        } else {
            out[pos++] = '%';
            out[pos++] = hex[(c >> 4) & 0x0F];
            out[pos++] = hex[c & 0x0F];
        }
    }
    out[pos] = '\0';
}


/**
 * @brief Parse a decimal string (e.g. "152.34", "-1.23") into a fixed-point
 * integer scaled by 100.
 *
 * @param s Null-terminated decimal string to parse.
 * @return Parsed value scaled by 100 (e.g. "12.34" -> 1234).
 */
static long parse_decimal(const char *s)
{
    long whole = 0, frac = 0;
    uint8_t neg = 0, frac_digits = 0;
    const char *p = s;

    if (*p == '-') { neg = 1; p++; }
    while (*p >= '0' && *p <= '9')
        whole = whole * 10 + (*p++ - '0');
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9' && frac_digits < 2) {
            frac = frac * 10 + (*p++ - '0');
            frac_digits++;
        }
        if (frac_digits == 1) frac *= 10;
    }
    return neg ? -(whole * 100 + frac) : (whole * 100 + frac);
}

/* -----------------------------------------------------------------------
 * Finnhub API calls
 * ----------------------------------------------------------------------- */

/**
 * @brief Fetch the current quote for a single stock and update its fields.
 *
 * Queries the Finnhub /quote endpoint and fills the price, change, and
 * change_pct fields of the supplied Stock record.
 *
 * @param s Pointer to the Stock record to update; symbol must be set.
 */
void get_stock_quote(Stock *s)
{
    sprintf(net_url, FINNHUB_STOCK_QUOTE, s->symbol, FINNHUB_API_KEY);
    check_error(network_open(net_url, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE),
                "Quote open failed");
    check_error(network_json_parse(net_url), "Quote parse failed");

    net_val[0] = '\0';
    network_json_query(net_url, "/c", net_val);
    s->price = parse_decimal(net_val);

    net_val[0] = '\0';
    network_json_query(net_url, "/d", net_val);
    s->change = parse_decimal(net_val);

    net_val[0] = '\0';
    network_json_query(net_url, "/dp", net_val);
    s->change_pct = (int)parse_decimal(net_val);

    network_close(net_url);
}

/**
 * @brief Fill the global stock_info struct with company profile data for symbol.
 *
 * Queries the Finnhub /stock/profile2 endpoint and populates all fields
 * of the global stock_info record.  Float fields (marketCapitalization,
 * shareOutstanding) are truncated to whole millions via atol().
 *
 * @param symbol Ticker symbol to look up.
 */
void get_stock_info(const char *symbol)
{
    memset(&stock_info, 0, sizeof(StockInfo));
    strncpy(stock_info.symbol, symbol, SYMBOL_LEN - 1);

    set_progress_message("Retrieving Info");
    sprintf(net_url, FINNHUB_COMPANY_PROFILE, symbol, FINNHUB_API_KEY);
    check_error(network_open(net_url, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE),
                "Company info open failed");
    update_progress_message();

    check_error(network_json_parse(net_url), "Company info parse failed");
    update_progress_message();

    net_val[0] = '\0';
    network_json_query(net_url, "/name", net_val);
    strncpy(stock_info.name, net_val, sizeof(stock_info.name) - 1);
    update_progress_message();

    net_val[0] = '\0';
    network_json_query(net_url, "/country", net_val);
    strncpy(stock_info.country, net_val, sizeof(stock_info.country) - 1);

    net_val[0] = '\0';
    network_json_query(net_url, "/currency", net_val);
    strncpy(stock_info.currency, net_val, sizeof(stock_info.currency) - 1);

    net_val[0] = '\0';
    network_json_query(net_url, "/exchange", net_val);
    strncpy(stock_info.exchange, net_val, sizeof(stock_info.exchange) - 1);

    net_val[0] = '\0';
    network_json_query(net_url, "/finnhubIndustry", net_val);
    strncpy(stock_info.industry, net_val, sizeof(stock_info.industry) - 1);

    net_val[0] = '\0';
    network_json_query(net_url, "/ipo", net_val);
    strncpy(stock_info.ipo, net_val, sizeof(stock_info.ipo) - 1);

    net_val[0] = '\0';
    network_json_query(net_url, "/phone", net_val);
    strncpy(stock_info.phone, net_val, sizeof(stock_info.phone) - 1);

    net_val[0] = '\0';
    network_json_query(net_url, "/weburl", net_val);
    strncpy(stock_info.weburl, net_val, sizeof(stock_info.weburl) - 1);

    net_val[0] = '\0';
    network_json_query(net_url, "/marketCapitalization", net_val);
    stock_info.market_cap = atol(net_val);

    net_val[0] = '\0';
    network_json_query(net_url, "/shareOutstanding", net_val);
    stock_info.shares_outstanding = atol(net_val);

    network_close(net_url);
}

/**
 * @brief Fill global lookup_results with up to MAX_LOOKUP_RESULTS ticker matches.
 *
 * API endpoint: https://finnhub.io/api/v1/search?q=<query>&token=<key>
 *
 * @param query    Search string (e.g. partial symbol or company name).
 */
void lookup_stock_ticker(const char *query)
{
    uint8_t i, count;

    memset(&lookup_results, 0, sizeof(LookupResults));

    url_encode(query, net_encoded, sizeof(net_encoded));
    sprintf(net_url, FINNHUB_SYMBOL_LOOKUP, net_encoded, FINNHUB_API_KEY);

    check_error(network_open(net_url, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE), "Symbol lookup open failed");
    update_progress_message();

    check_error(network_json_parse(net_url), "Symbol lookup parse failed");
    update_progress_message();

    net_val[0] = '\0';
    network_json_query(net_url, "/count", net_val);
    count = (uint8_t)atoi(net_val);
    if (count > MAX_LOOKUP_RESULTS)
        count = MAX_LOOKUP_RESULTS;
    lookup_results.count = count;

    for (i = 0; i < count; i++) {
        update_progress_message();
        snprintf(net_path, sizeof(net_path), "/result/%d/description", i);
        net_val[0] = '\0';
        network_json_query(net_url, net_path, net_val);
        strncpy(lookup_results.results[i].description, net_val,
                sizeof(lookup_results.results[i].description) - 1);

        snprintf(net_path, sizeof(net_path), "/result/%d/displaySymbol", i);
        net_val[0] = '\0';
        network_json_query(net_url, net_path, net_val);
        strncpy(lookup_results.results[i].displaySymbol, net_val,
                sizeof(lookup_results.results[i].displaySymbol) - 1);
    }

    network_close(net_url);
}
