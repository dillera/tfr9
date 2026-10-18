#ifndef STOCKS_H
#define STOCKS_H

/* -----------------------------------------------------------------------
 * stocks.h
 * Main header for the FujiNet Stocks application.
 * Multiplatform 8-bit C (cc65 / cmoc).
 * ----------------------------------------------------------------------- */

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include <screen.h>
#include <chardefs.h>

/* Auto-refresh interval: fetch new quotes every 5 minutes (300 * CLOCKS_PER_SEC) */
#define GET_QUOTES_INTERVAL  ((clock_t)(300L * CLOCKS_PER_SEC))

/* Number of stocks the app tracks */
#define MAX_STOCKS         10

/* Maximum length of a stock ticker symbol (plus null terminator) */
#define SYMBOL_LEN         11

/* -----------------------------------------------------------------------
 * Coord  –  a 2-D screen coordinate (x = column, y = row)
 * ----------------------------------------------------------------------- */
typedef struct {
    uint8_t x;
    uint8_t y;
} Coord;

/* -----------------------------------------------------------------------
 * Stock  –  the live data record kept for each tracked symbol.
 *
 * Prices and changes are stored scaled by 100 to avoid floating-point
 * on 8-bit targets.  E.g. 12.34 is stored as 1234.
 * change and change_pct are signed: positive = up, negative = down.
 * change_pct holds the percentage * 100 so 1.23% = 123, -1.23% = -123.
 * ----------------------------------------------------------------------- */
typedef struct {
    char           symbol[SYMBOL_LEN];   /**< ticker symbol, NUL-terminated      */
    long           price;                /**< current price * 100     (API: c)    */
    long           change;               /**< signed change * 100     (API: d)    */
    int            change_pct;           /**< signed pct change * 100 (API: dp)   */
} Stock;

/* -----------------------------------------------------------------------
 * StockInfo  –  company profile returned by the Company Profile 2 API.
 * Filled in by get_stock_info() in network.c; displayed on the info
 * screen.  The caller owns the storage.
 *
 * Float fields (marketCapitalization, shareOutstanding) are stored as
 * long integers (truncated) to avoid floating-point on 8-bit targets.
 * Both are reported in millions by the API and stored as millions here.
 * ----------------------------------------------------------------------- */
typedef struct {
    char           symbol[SYMBOL_LEN];   /* ticker symbol (API: ticker)           */
    char           name[41];             /* company name  (API: name)             */
    char           country[3];           /* ISO 2-letter country code (API: country) */
    char           currency[4];          /* ISO 3-letter currency code (API: currency) */
    char           exchange[21];         /* exchange name (API: exchange)         */
    char           industry[31];         /* industry      (API: finnhubIndustry)  */
    char           ipo[11];              /* IPO date "YYYY-MM-DD" (API: ipo)      */
    long           market_cap;           /* market cap in millions (API: marketCapitalization) */
    long           shares_outstanding;   /* shares outstanding in millions (API: shareOutstanding) */
    char           phone[21];            /* phone number  (API: phone)            */
    char           weburl[51];           /* web URL       (API: weburl)           */
} StockInfo;

/* -----------------------------------------------------------------------
 * Global arrays (defined in stocks.c)
 * ----------------------------------------------------------------------- */

/** @brief The list of stocks being tracked. */
extern Stock stocks[MAX_STOCKS];

/**
 * @brief Pre-computed screen positions for each stock slot.
 *
 * Two columns of five rows, centred within SCREEN_WIDTH.
 *   Slots 0-4 -> left  column (x = SLOT_LEFT_X)
 *   Slots 5-9 -> right column (x = SLOT_RIGHT_X)
 *   SLOT_GAP (5) chars between labels.
 *   Rows 5, 8, 11, 14, 17 (2 blank rows between each pair).
 */
extern const Coord stock_coords[MAX_STOCKS];


/* -----------------------------------------------------------------------
 * LookupResult / LookupResults  –  returned by lookup_stock_ticker().
 * Up to 10 matches, each carrying the displaySymbol shown on exchange
 * and the human-readable description.
 * ----------------------------------------------------------------------- */
#define MAX_LOOKUP_RESULTS  10

typedef struct {
    char description[41];    /* e.g. "APPLE INC"           */
    char displaySymbol[12];  /* e.g. "AAPL", "AAPL.SW"    */
} LookupResult;

typedef struct {
    uint8_t count;
    LookupResult results[MAX_LOOKUP_RESULTS];
} LookupResults;

/* -----------------------------------------------------------------------
 * Function prototypes
 * ----------------------------------------------------------------------- */

/* Platform I/O — implemented per-platform in src/<platform>/conio.c */
void sync_frame(void);

void main_loop(void);
void build_scroll_string(char *buf, int buf_len);
void format_price(long cents, char *out, uint8_t out_len);
void sort_stocks(void);
void delete_stock(uint8_t index);
void load_stocks(void);
void save_stocks(void);
clock_t get_stock_quotes(void);
void set_progress_message(const char *msg);
void update_progress_message(void);

#endif /* STOCKS_H */
