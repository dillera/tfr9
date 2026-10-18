#ifndef NETWORK_H
#define NETWORK_H

/* -----------------------------------------------------------------------
 * network.h
 * FujiNet network calls for retrieving stock data.
 * ----------------------------------------------------------------------- */

#include <stdint.h>
#include <stdbool.h>
#include "stocks.h"

#define FINNHUB_API_KEY         "d6gunihr01qoor9mjbsgd6gunihr01qoor9mjbt0"
#define FINNHUB_COMPANY_PROFILE "N1:HTTPS://finnhub.io/api/v1/stock/profile2?symbol=%s&token=%s"
#define FINNHUB_STOCK_QUOTE     "N1:HTTPS://finnhub.io/api/v1/quote?symbol=%s&token=%s"
#define FINNHUB_SYMBOL_LOOKUP   "N1:HTTPS://finnhub.io/api/v1/search?q=%s&token=%s"

extern LookupResults lookup_results;
extern StockInfo     stock_info;

void check_error(uint8_t err, const char *msg);
void url_encode(const char *src, char *out, int out_len);
void get_stock_quote(Stock *s);
void get_stock_info(const char *symbol);
void lookup_stock_ticker(const char *query);

#endif /* NETWORK_H */
