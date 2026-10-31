/* The display for the TFR/911H: the tether's terminal, 80 by 24, in
 * xterm's 256 colours (screen.c).  Its own layout, using all 80 columns:
 *
 *   row  0     the title bar: the game's name, the region and the link
 *   1-10       the Grid (left), each node three columns wide on a dark
 *              ground: . empty, + a lone spike, 1-6 where that exploit can
 *              run, a hive's letter on its syndicate's colour (white for
 *              Black ICE); and the hives (right): each syndicate's letter,
 *              its hive's size, a bar to the 41 that ends the game, and
 *              its name, or while buying its price and what is left
 *   11         the last move (its node blinks for three seconds)
 *   12-18      the netrunners: cash, and each syndicate's crypto in its colour
 *   19-21      the log, each line's [x] in a colour of its own
 *   22-23      the kit and the question; on a purple ground while this
 *              seat has a decision to make; the clock at the right
 *
 * The title is the Atari's logo, its columns doubled as in 80 columns on
 * MS-DOS (title.py writes title.h), its outlines in neon, and TV static
 * crawling in its letters (display_title_idle).
 *
 * The instructions are pages of text in SYS/synack.hlp, 24 lines of at
 * most 40 characters each, drawn over the middle of the screen; { } marks
 * a highlight and \ the yen, as on the other machines.  While they are up
 * the game goes on, but draws nothing; closing them draws it all again,
 * from the game's state and the log kept here.
 *
 * @license gpl v. 3, see fujinet/LICENSE for details. */
#include <string.h>
#include "screen.h"
#include "../config.h"
#include "../platform.h"
#include "../display.h"
#include "../common/proto.h"
#include "../common/proto_tab.h"
#include "../common/game.h"
#include "title.h"

unsigned char snd_is_on(void);          /* sound.c */

/* ---------------------------------------------------------------- colours */
#define K_BLACK   16
#define C_WHITE   231
#define C_TEXT    252
#define C_DIM     244
#define C_FAINT   239
#define C_NEON    51                    /* cyan */
#define C_PINK    205
#define C_HOT     201                   /* magenta */
#define C_GOLD    220
#define C_GREEN   46
#define C_RED     196
#define C_AMBER   214
#define C_YELLOW  226
#define BG_BAR    17                    /* the title bar: navy */
#define BG_GRID   234                   /* the Grid's ground */
#define BG_SPIKE  238
#define BG_TRACK  235                   /* under a hive's bar */
#define BG_TURN   236                   /* the row of the seat whose turn it is */
#define BG_ASK    53                    /* rows 22-23 while this seat decides */
#define BG_LAST   C_HOT                 /* the last move, lit */
#define BG_HELP   233
#define BG_MARK   54                    /* the instructions' { } */
#define BG_ICE    25

/* each syndicate's, in theme order (G R H I B L O) */
static const unsigned char syn_colour[NSYN] = { 48, 196, 220, 39, 208, 205, 141 };

/* ---------------------------------------------------------------- layout */
#define ROW_BOARD   2
#define COL_LABEL   2
#define COL_BOARD   5                   /* node column c is at COL_BOARD + 3c */
#define COL_PANEL   44                  /* the hives: letter, size, bar, name, ICE */
#define COL_SIZE    48
#define COL_BAR     51
#define BAR_CELLS   8
#define COL_NAME    60
#define COL_ICE     76
#define ROW_LAST    (ROW_BOARD + RULE_ROWS)
#define ROW_SEATS   13
#define COL_NAME_S  3
#define COL_CASH    12                  /* 10 wide */
#define COL_COINS   26                  /* 5 a syndicate */
#define COL_STATE   63
#define ROW_LOG     19
#define LOG_ROWS    3
#define LOG_COLS    78
#define ROW_INFO    22
#define ROW_ASK     23
#define COL_CLOCK   75

#define ROW_LOGO    1
#define ROW_TAG     (ROW_LOGO + TITLE_ROWS)
#define ROW_REGION  17
#define COL_FIELD   28
#define ROW_KEYS    21

#define FLASH_TICKS 180                 /* how long the last move blinks */

static unsigned char ready, field, helping, i, j, n, v;
static unsigned char bottom = C_NONE;   /* rows 22-23's ground */
static unsigned char secs_left;         /* the clock, as last shown */
static char log[LOG_ROWS][LOG_COLS + 1];

static void setup(void)
{
    if (ready)
        return;
    ready = 1;
    scr_init();
}

#ifdef SAW_DEBUG
/* Where it has got to, at the screen's bottom right corner. */
void debug_mark(char c)
{
    char s[2];
    s[0] = c;
    s[1] = 0;
    scr_at(23, 78);
    scr_ink(K_BLACK, C_GOLD);
    scr_raw(s);
    scr_flush();
}
#endif

/* Back to the shell: the terminal as it was. */
void display_restore(void)
{
    if (ready)
        scr_done();
}

/* ---------------------------------------------------------------- pieces */

static void clear_row(unsigned char r, unsigned char bg)
{
    scr_at(r, 0);
    scr_ink(C_TEXT, bg);
    scr_erase_row();
}

/* Right-aligned decimal in `width` columns.  `money` (units of 100) adds
 * "00", the thousands' commas, and the yen sign before the digits. */
static void number(unsigned value, unsigned char width, unsigned char money)
{
    char d[12];
    unsigned char k = 0, g = 0;
    if (money && value) {
        d[k++] = '0';
        d[k++] = '0';
        g = 2;
    }
    do {
        if (money && g == 3) {
            d[k++] = ',';
            g = 0;
        }
        d[k++] = (char)('0' + value % 10);
        value /= 10;
        ++g;
    } while (value);
    if (money)
        d[k++] = '\\';
    if (width > k)
        scr_spaces((unsigned char)(width - k));
    while (k)
        scr_put(d[--k]);
}

static void node_name(unsigned char node)
{
    for (n = 0; node >= RULE_COLS; ++n)
        node -= RULE_COLS;
    scr_put((char)('A' + node));
    scr_put((char)('0' + n));
}

/* A syndicate's letter as a chip: bold black on its colour. */
static void chip(unsigned char s, unsigned char bg_after)
{
    scr_bold(1);
    scr_ink(K_BLACK, syn_colour[s]);
    scr_put(' ');
    scr_put(syn_letter[s]);
    scr_put(' ');
    scr_bold(0);
    scr_ink(C_TEXT, bg_after);
}

static unsigned char syn_of(unsigned char c)
{
    for (v = 0; v < NSYN; ++v)
        if (syn_letter[v] == c)
            return v;
    return 0;
}

/* The syndicate letters in `mask`, as chips. */
static void syn_letters(unsigned char mask)
{
    for (i = 0; i < NSYN; ++i)
        if (mask >> i & 1) {
            chip(i, bottom);
            scr_put(' ');
        }
}

/* Keys on the title, and in the bar: the key bright, then what it does. */
static void key_label(const char *key, const char *what, unsigned char bg)
{
    scr_bold(1);
    scr_ink(C_NEON, bg);
    scr_text(key);
    scr_bold(0);
    scr_ink(C_DIM, bg);
    scr_put(' ');
    scr_text(what);
}

/* ---------------------------------------------------------------- the title */

static const char static_glyphs[] =     /* three bytes each: light to solid, mostly dense */
    "\xE2\x96\x91" "\xE2\x96\x92" "\xE2\x96\x92" "\xE2\x96\x93"
    "\xE2\x96\x93" "\xE2\x96\x93" "\xE2\x96\x88" "\xE2\x96\x88";
static const unsigned char neon_rows[TITLE_ROWS] =
    { 51, 45, 39, 33, 69, 63, 99, 135, 171, 207, 206, 205, 204, 198 };
static unsigned char seed = 0x5A, frame;

static unsigned char rnd(void)
{
    seed = (unsigned char)((seed << 1) ^ ((seed & 0x80) ? 0x1D : 0));
    return seed;
}

/* One run of a letter's stroke, filled with static: a grey, or one time
 * in four its row's neon, in shades and blocks. */
static void static_run(unsigned char k)
{
    const unsigned char *r = title_runs + 3 * k;
    char g[4];
    unsigned char c, x;
    x = rnd();
    c = (x & 7) < 2 ? neon_rows[r[0]] : (unsigned char)(244 + (x >> 4) % 12);
    scr_at((unsigned char)(ROW_LOGO + r[0]), r[1]);
    scr_ink(c, C_NONE);
    g[3] = 0;
    for (n = 0; n < r[2]; ++n) {
        memcpy(g, static_glyphs + 3 * (rnd() & 7), 3);
        scr_glyph(g);
    }
}

static void sound_state(void)
{
    scr_at(ROW_KEYS, 26);
    key_label("^S", "SOUND", C_NONE);
    scr_ink(C_WHITE, C_NONE);
    scr_text(snd_is_on() ? " ON " : " OFF");
}

/* ^S, from input.c: the title's ON/OFF follows. */
void display_sound(void)
{
    if (ready && !field && !helping) {
        sound_state();
        scr_flush();
    }
}

void display_title_screen(void)
{
    setup();
    field = 0;
    if (helping)
        return;
    scr_clear();
    scr_at(ROW_LOGO, 0);
    scr_raw(TITLE_ART);
    for (j = 0; j < TITLE_RUNS; ++j)
        static_run(j);
    scr_at(ROW_TAG, 12);
    scr_ink(37, C_NONE);
    scr_text("A NETWORK GAME OF HACKING AND CRYPTO FOR 2-6 NETRUNNERS");
    scr_at(ROW_REGION, COL_FIELD - 8);
    scr_ink(C_PINK, C_NONE);
    scr_text("REGION");
    scr_at(ROW_REGION + 1, COL_FIELD - 8);
    scr_text("STATUS");
    scr_at(ROW_REGION + 2, COL_FIELD - 8);
    scr_text("HANDLE");
    scr_at(ROW_KEYS, 4);
    key_label("TAB", "MOVE HINTS", C_NONE);
    sound_state();
    scr_at(ROW_KEYS, 43);
    key_label("?", "INSTRUCTIONS", C_NONE);
    scr_at(ROW_KEYS, 63);
    key_label("ESC", "QUIT", C_NONE);
    if (refused) {
        clear_row(ROW_ASK, 124);
        scr_at(ROW_ASK, 2);
        scr_bold(1);
        scr_ink(C_WHITE, 124);
        scr_text(refused == REFUSED_LATE ? "TOO LATE: THE LAST SEAT WAS TAKEN"
                 : "THAT HANDLE IS TAKEN: TYPE ANOTHER");
        scr_bold(0);
    } else {
        scr_at(ROW_ASK, 2);
        scr_ink(C_DIM, C_NONE);
        scr_text("TYPE A HANDLE, THEN ENTER. ESC QUITS");
    }
    display_title_update();
}

/* Each field is padded to its widest, so drawing over the last one leaves nothing behind. */
void display_title_update(void)
{
    if (helping)
        return;
    scr_at(ROW_REGION, COL_FIELD);
    scr_bold(1);
    scr_ink(C_WHITE, C_NONE);
    scr_textn(region, region_len);
    scr_bold(0);
    scr_spaces((unsigned char)(sizeof region - region_len));
    scr_at(ROW_REGION + 1, COL_FIELD);
    n = 0;
    if (status_len) {
        n = status_len - 1;
        scr_ink(status_code == ST_FULL ? C_AMBER : C_GREEN, C_NONE);
        scr_textn(status_text, n);
    } else if (status_code == NO_CARRIER) {
        n = 10;
        scr_ink(C_RED, C_NONE);
        scr_text("NO CARRIER");
    } else {
        n = 10;
        scr_ink(C_DIM, C_NONE);
        scr_text("DIALING...");
    }
    scr_spaces((unsigned char)(sizeof status - 2 - n));
    scr_at(ROW_REGION + 2, COL_FIELD);
    scr_bold(1);
    scr_ink(C_NEON, C_NONE);
    scr_textn(edit, edit_len);
    scr_bold(0);
    scr_ink(K_BLACK, C_NEON);           /* the cursor */
    scr_put(' ');
    scr_ink(C_TEXT, C_NONE);
    scr_spaces((unsigned char)(8 - edit_len));
    scr_at(ROW_KEYS, 19);
    scr_ink(C_WHITE, C_NONE);
    scr_text(hints ? "ON " : "OFF");
    scr_flush();
}

/* The static crawls: a few runs at a time, about 15 times a second. */
void display_title_idle(void)
{
    unsigned char t = ticks();
    if (helping || field || (unsigned char)(t - frame) < 4)
        return;
    frame = t;
    for (j = 0; j < 3; ++j)
        static_run((unsigned char)(rnd() % TITLE_RUNS));
    scr_flush();
}

#if !FANCY_PLAYFIELD
/* ---------------------------------------------------------------- playfield */

/* The last move blinks from when it is first drawn until FLASH_TICKS have
 * passed (display_idle); then `done` holds it, and it shows as usual. */
static unsigned char blink = NONE, blink_t, done = NONE, lit;

/* The key ('1'-'6') of the exploit in my kit that can run on `node` now, or 0. */
static unsigned char target(unsigned char node)
{
    if (!hints)
        return 0;
    for (n = 0; n < RULE_KITSIZE; ++n)
        if (kit[n] == node)             /* bit 7 set = can't run: never equal */
            return (unsigned char)('1' + n);
    return 0;
}

/* The last move, under the Grid; label_shown is the node it names. */
static unsigned char label_shown = NONE;

static void last_label(void)
{
    label_shown = last_node;
    scr_at(ROW_LAST, COL_BOARD);
    if (last_node == NONE) {
        scr_ink(C_TEXT, C_NONE);
        scr_spaces(12);
        return;
    }
    scr_ink(C_DIM, C_NONE);
    scr_text("LAST MOVE ");
    scr_ink(C_WHITE, C_NONE);
    node_name(last_node);
}

/* A node's three columns, at the cursor: its colours change only where
 * they must, so a run of empty nodes costs nothing but the characters. */
static void node_cell(unsigned char node)
{
    unsigned char fg, bg, ch, bold = 0;
    v = board[node];
    if (v == CELL_EMPTY) {
        ch = target(node);
        if (ch) {
            fg = C_YELLOW;
            bold = 1;
        } else {
            ch = '.';
            fg = C_FAINT;
        }
        bg = BG_GRID;
    } else if (v == CELL_SPIKE) {
        ch = '+';
        fg = C_WHITE;
        bg = BG_SPIKE;
        bold = 1;
    } else {
        v -= 2;
        ch = syn_letter[v];
        bg = syn_colour[v];
        fg = hive_size[v] >= RULE_SAFESIZE ? C_WHITE : K_BLACK;
        bold = 1;
    }
    if (node == last_node) {
        if (node != done) {
            if (blink != node) {
                blink = node;
                blink_t = ticks();
                lit = 1;
            }
            if (lit) {
                fg = C_WHITE;
                bg = BG_LAST;
            }
        }
    } else if (node == done)
        done = NONE;                    /* no longer the last move: it may be again, next game */
    scr_bold(bold);
    scr_ink(fg, bg);
    scr_put(' ');
    scr_put((char)ch);
    scr_put(' ');
}

void display_node(unsigned char node)
{
    unsigned char r, c = node;
    if (helping || !field)
        return;
    for (r = 0; c >= RULE_COLS; ++r)
        c -= RULE_COLS;
    scr_at((unsigned char)(ROW_BOARD + r), (unsigned char)(COL_BOARD + 3 * c));
    node_cell(node);
    scr_bold(0);
    if (node == last_node && label_shown != node)
        last_label();
    scr_flush();
}

void display_idle(void)
{
    if (blink == NONE || helping)
        return;
    if (blink != last_node)
        blink = NONE;                   /* a newer move took over, or a new game */
    else if ((unsigned char)(v = ticks() - blink_t) >= FLASH_TICKS) {
        done = blink;
        blink = NONE;
        display_node(done);
    } else if ((v >> 5 & 1) == lit) {   /* lit and unlit by turns, about twice a second */
        lit ^= 1;
        display_node(blink);
    }
}

/* A hive's size as a bar to RULE_ENDSIZE, in eighths of a cell. */
static void bar(unsigned char size, unsigned char colour)
{
    static const char eighths[] =       /* 1/8 to 7/8, three bytes each */
        "\xE2\x96\x8F" "\xE2\x96\x8E" "\xE2\x96\x8D" "\xE2\x96\x8C"
        "\xE2\x96\x8B" "\xE2\x96\x8A" "\xE2\x96\x89";
    char g[4];
    unsigned char k, units = size >= RULE_ENDSIZE ? 8 * BAR_CELLS
                             : (unsigned char)((unsigned)size * (8 * BAR_CELLS) / RULE_ENDSIZE);
    if (size && !units)
        units = 1;
    scr_ink(colour, BG_TRACK);
    g[3] = 0;
    for (k = 0; k < BAR_CELLS; ++k) {
        if (units >= 8) {
            scr_glyph("\xE2\x96\x88");
            units -= 8;
        } else if (units) {
            memcpy(g, eighths + 3 * (units - 1), 3);
            scr_glyph(g);
            units = 0;
        } else
            scr_put(' ');
    }
}

/* What each row of the hives shows now: its size, and whether the prices
 * are up; NONE when it must be drawn again. */
static unsigned char hive_shown[NSYN], prices_shown = NONE;

static void hives_unknown(void)
{
    memset(hive_shown, NONE, sizeof hive_shown);
    prices_shown = NONE;
}

void display_hives(void)
{
    unsigned char prices = pr_kind == 'B';
    if (helping || !field)
        return;
    if (prices != prices_shown) {
        memset(hive_shown, NONE, sizeof hive_shown);
        prices_shown = prices;
    }
    scr_at(1, COL_PANEL);
    scr_bold(1);
    scr_ink(C_NEON, C_NONE);
    scr_text(T_HIVE_P);
    scr_bold(0);
    scr_spaces(COL_BAR - COL_PANEL - (sizeof T_HIVE_P - 1));
    scr_ink(C_DIM, C_NONE);
    scr_text("SIZE    ");
    scr_spaces(COL_NAME - COL_BAR - 8);
    scr_text(pr_kind == 'B' ? "  PRICE    LEFT  " : "SYNDICATE        ");
    for (j = 0; j < NSYN; ++j) {
        n = hive_size[j];
        if (n == hive_shown[j])
            continue;
        hive_shown[j] = n;
        scr_at((unsigned char)(ROW_BOARD + j), COL_PANEL);
        chip(j, C_NONE);
        scr_put(' ');
        if (n) {
            scr_bold(n >= RULE_SAFESIZE);
            scr_ink(C_WHITE, C_NONE);
            number(n, 2, 0);
            scr_bold(0);
        } else {
            scr_ink(C_FAINT, C_NONE);
            scr_text(" -");
        }
        scr_put(' ');
        bar(n, syn_colour[j]);
        scr_ink(C_TEXT, C_NONE);
        scr_put(' ');
        if (pr_kind == 'B') {           /* pr: timeout, cash[2], flags, price[7], supply[7] */
            v = pr[4 + j];
            if (v) {
                scr_ink(C_GOLD, C_NONE);
                number(v, 7, 1);
                scr_ink(C_TEXT, C_NONE);
                number(pr[11 + j], 3, 0);
                scr_ink(C_DIM, C_NONE);
                scr_text(" LEFT");
            } else if (n >= RULE_SAFESIZE)
                scr_spaces(15);
        } else {
            scr_ink(syn_colour[j], C_NONE);
            scr_text(syn_name[j]);
            if (n >= RULE_SAFESIZE)
                scr_spaces((unsigned char)(15 - strlen(syn_name[j])));
        }
        if (n >= RULE_SAFESIZE) {
            scr_put(' ');
            scr_bold(1);
            scr_ink(C_WHITE, BG_ICE);
            scr_text("ICE");
            scr_bold(0);
        } else {
            scr_ink(C_TEXT, C_NONE);
            scr_eol();
        }
    }
    scr_flush();
}

void display_seat(unsigned char seat)
{
    unsigned char bg;
    if (seat >= SEATS || helping || !field)
        return;
    bg = seat == turn ? BG_TURN : C_NONE;
    scr_at((unsigned char)(ROW_SEATS + seat), 0);
    scr_ink(C_NEON, bg);
    scr_put(' ');
    if (seat == me)
        scr_glyph("\xE2\x96\xB6");      /* the play arrowhead: you */
    else
        scr_put(' ');
    scr_put(' ');
    n = seat_kind[seat];
    if (n == SEAT_EMPTY) {
        scr_ink(C_FAINT, bg);
        scr_put('-');
        scr_eol();
        scr_flush();
        return;
    }
    scr_bold(seat == turn);
    scr_ink(seat == turn ? C_WHITE : C_TEXT, bg);
    scr_textn((const unsigned char *)seat_name[seat], 8);
    scr_bold(0);
    scr_ink(n == SEAT_AWAY ? C_AMBER : C_DIM, bg);
    scr_put(n == SEAT_AWAY ? '?' : n == SEAT_BOT ? '*' : ' ');
    scr_ink(C_GOLD, bg);
    number(cash[seat], 10, 1);
    scr_spaces(COL_COINS - COL_CASH - 10);
    for (j = 0; j < NSYN; ++j) {
        v = coins[seat][j];
        if (v) {
            scr_ink(syn_colour[j], bg);
            number(v, 3, 0);
        } else {
            scr_ink(C_FAINT, bg);
            scr_text("  .");
        }
        scr_spaces(2);
    }
    scr_spaces(COL_STATE - COL_COINS - 5 * NSYN);
    if (seat == turn) {
        scr_bold(1);
        scr_ink(C_NEON, bg);
        scr_glyph("\xE2\x97\x80");      /* the arrowhead, back at the name */
        scr_text(seat == me ? " YOUR TURN" : " THEIR TURN");
        scr_bold(0);
        scr_eol();
    } else {
        scr_ink(C_DIM, bg);
        scr_text(n == SEAT_BOT ? "BOT" : n == SEAT_AWAY ? "AWAY" : seat == me ? "YOU" : "");
        scr_eol();
    }
    scr_flush();
}

/* The log's colours, by the character in its line's [ ]. */
static unsigned char log_colour(char c)
{
    switch (c) {
    case '+': return C_WHITE;
    case '*': return C_NEON;
    case '$': return C_GREEN;
    case '!': return C_HOT;
    case '#': return 75;
    case '?': return C_YELLOW;
    }
    return C_DIM;
}

static void log_row(unsigned char k)
{
    const char *s = log[k];
    scr_at((unsigned char)(ROW_LOG + k), 0);
    scr_ink(C_TEXT, C_NONE);
    scr_put(' ');
    n = (unsigned char)strlen(s);
    if (n >= 3 && s[0] == '[' && s[2] == ']') {
        scr_bold(1);
        scr_ink(log_colour(s[1]), C_NONE);
        scr_textn((const unsigned char *)s, 3);
        scr_bold(0);
        scr_ink(C_TEXT, C_NONE);
        scr_text(s + 3);
    } else {
        scr_ink(C_TEXT, C_NONE);
        scr_text(s);
    }
    scr_eol();
}

void display_log(void)
{
    for (i = 0; i < LOG_ROWS - 1; ++i)
        memcpy(log[i], log[i + 1], LOG_COLS + 1);
    n = msg[1];
    if (n > LOG_COLS)
        n = LOG_COLS;
    memcpy(log[LOG_ROWS - 1], msg + 2, n);
    log[LOG_ROWS - 1][n] = 0;
    if (helping || !field)
        return;
    scr_ink(C_TEXT, C_NONE);            /* the row the scroll brings in is blank */
    scr_sync();
    scr_raw("\033[" "20;22" "r");       /* the log's rows (ROW_LOG + 1 to + LOG_ROWS) */
    scr_at(ROW_LOG + LOG_ROWS - 1, 0);
    scr_raw("\n");                      /* scroll them up a row */
    scr_raw("\033[1;24r");
    log_row(LOG_ROWS - 1);
    scr_flush();
}

static void kit_line(void)
{
    scr_at(ROW_INFO, 1);
    scr_bold(1);
    scr_ink(C_PINK, bottom);
    scr_text(T_EXPLOIT_P);
    scr_bold(0);
    for (j = 0; j < RULE_KITSIZE; ++j) {
        v = kit[j];
        scr_ink(C_TEXT, bottom);
        scr_spaces(2);
        scr_ink(v == NONE || (v & 0x80) ? C_FAINT : C_YELLOW, bottom);  /* can't run now: faint */
        scr_put((char)('1' + j));
        scr_put(':');
        if (v == NONE)
            scr_text("--");
        else
            node_name(v & 0x7F);
    }
}

/* The question, on row 23: a complaint instead, should there be one. */
static void ask(const char *s)
{
    scr_at(ROW_ASK, 1);
    if (complaint) {
        scr_bold(1);
        scr_ink(C_AMBER, bottom);
        scr_text(complaint);
    } else {
        scr_bold(1);
        scr_ink(C_WHITE, bottom);
        scr_text(s);
    }
    scr_bold(0);
}

static unsigned char clock_shown;       /* the seconds it shows, 0 for none */

static void clock_digits(void)
{
    clock_shown = pr_kind ? secs_left : 0;
    scr_at(ROW_ASK, COL_CLOCK);
    if (!pr_kind || !secs_left) {
        scr_ink(C_TEXT, bottom);
        scr_spaces(4);
        return;
    }
    scr_bold(1);
    scr_ink(secs_left <= 11 ? C_RED : C_YELLOW, bottom);
    number((unsigned)(secs_left - 1), 3, 0);
    scr_put('S');
    scr_bold(0);
}

void display_prompt(void)
{
    if (helping || !field)
        return;
    bottom = pr_kind || turn == me || leaving ? BG_ASK : C_NONE;
    clear_row(ROW_INFO, bottom);
    clear_row(ROW_ASK, bottom);
    if (pr_kind == 'P' || (pr_kind == 0 && turn != NONE))
        kit_line();
    else if (pr_kind == 0 && winners) {         /* in the lobby: the last result stays */
        scr_at(ROW_INFO, 1);
        scr_bold(1);
        scr_ink(C_GOLD, bottom);
        scr_text(winners & (winners - 1) ? "TOP " T_PLAYER_P ":" : "GAME OVER. TOP " T_PLAYER ":");
        for (i = 0; i < SEATS; ++i)
            if (winners >> i & 1) {
                scr_put(' ');
                scr_textn((const unsigned char *)seat_name[i], 8);
            }
        scr_bold(0);
    }
    if (leaving) {                      /* Esc: the question waits; the prompt comes back after */
        ask(T_LOGOFF " NOW? Y/N");
        scr_flush();
        return;
    }
    scr_at(ROW_INFO, 1);
    scr_ink(C_TEXT, bottom);
    switch (pr_kind) {
    case 0:
        if (turn < SEATS && turn != me) {
            scr_at(ROW_ASK, 1);
            scr_ink(C_DIM, bottom);
            scr_text("WAITING FOR ");
            scr_ink(C_TEXT, bottom);
            scr_textn((const unsigned char *)seat_name[turn], 8);
            scr_at(ROW_ASK, 56);
            key_label("?", "THE INSTRUCTIONS", bottom);
        }
        scr_flush();
        return;
    case 'S':
        ask("START WITH HOW MANY SEATS? 2-6");
        break;
    case 'P':
        ask("RUN WHICH " T_EXPLOIT "? PRESS 1-6");
        break;
    case 'F':                           /* pr[1] = available */
        scr_bold(1);
        scr_ink(C_HOT, bottom);
        scr_text(T_INFILTRATION "! ");
        scr_bold(0);
        scr_ink(C_TEXT, bottom);
        scr_text(T_BROKER " TO ");
        syn_letters(pr[1]);
        ask("PRESS A SYNDICATE LETTER");
        break;
    case 'K':                           /* pr[1] = ctx, pr[2] = options */
        scr_bold(1);
        scr_ink(C_HOT, bottom);
        scr_text("TIE! ");
        scr_bold(0);
        scr_ink(C_TEXT, bottom);
        scr_text(pr[1] ? "ABSORB WHICH NEXT? " : "WHICH SURVIVES? ");
        syn_letters(pr[2]);
        ask("PRESS A SYNDICATE LETTER");
        break;
    case 'D':                           /* pr: absorbed, absorber, held, supply */
        scr_bold(1);
        scr_ink(C_HOT, bottom);
        scr_text(T_HIVEJACK ": ");
        scr_bold(0);
        chip(pr[2], bottom);
        scr_text(" TAKES ");
        chip(pr[1], bottom);
        scr_text(". YOU HOLD ");
        scr_ink(C_WHITE, bottom);
        number(pr[3], 1, 0);
        scr_put(' ');
        scr_ink(C_TEXT, bottom);
        scr_put(syn_letter[pr[1]]);
        ask(step ? T_SWAP " 2 FOR 1, HOW MANY? " : T_CASHOUT " HOW MANY? ");
        if (!complaint) {
            scr_bold(1);
            scr_ink(C_NEON, bottom);
            scr_textn(edit, edit_len);
            scr_bold(0);
        }
        break;
    case 'B':                           /* pr: cash[2], flags, price[7], supply[7] */
        if (!(step && !edit_len)) {     /* nothing bought: just the question */
            scr_text("BUY UP TO 3 " T_COIN ": ");
            for (j = 0; j < edit_len; ++j) {
                chip(syn_of(edit[j]), bottom);
                scr_put(' ');
            }
        }
        ask(step ? T_NULLROUTE " AND END THE GAME? Y/N"
                 : "LETTERS, BACKSPACE, ENTER TO BUY");
        break;
    }
    clock_digits();
    scr_flush();
}

/* One second short, as on the other machines: never promise more time
 * than the server gives. */
void display_clock(unsigned char secs)
{
    secs_left = secs;
    if (!pr_kind || helping || !field || secs == clock_shown)
        return;
    clock_digits();
    scr_flush();
}

void display_message(const char *s)
{
    setup();
    if (helping)
        return;
    clear_row(ROW_ASK, C_AMBER);
    scr_at(ROW_ASK, 1);
    scr_bold(1);
    scr_ink(K_BLACK, C_AMBER);
    scr_text(s);
    scr_bold(0);
    scr_ink(C_TEXT, C_NONE);
    scr_flush();
}

void display_game_over(void)
{
    display_prompt();                   /* the lobby's rows show the result */
}

void display_link(void)
{
    if (!field || helping)
        return;
    scr_at(0, 17);
    scr_bold(1);
    scr_ink(C_WHITE, BG_BAR);
    scr_textn(region, region_len);
    scr_bold(0);
    scr_ink(C_DIM, BG_BAR);
    scr_put(':');
    scr_bold(1);
    scr_ink(link_up ? C_GREEN : C_RED, BG_BAR);
    scr_text(link_up ? "CONNECTED" : "NO SIGNAL");
    scr_bold(0);
    scr_ink(C_TEXT, BG_BAR);
    scr_spaces((unsigned char)(sizeof region - region_len));
    scr_flush();
}

void display_playfield(void)
{
    static const unsigned char title_ink[12] =
        { 51, 51, 45, 45, 39, 39, 99, 135, 171, 207, 205, 198 };
    setup();
    field = 1;
    if (helping)
        return;
    scr_clear();
    clear_row(0, BG_BAR);
    scr_at(0, 1);
    scr_bold(1);
    for (j = 0; j < sizeof TH_TITLE - 1 && j < 12; ++j) {
        scr_ink(title_ink[j], BG_BAR);
        scr_put(TH_TITLE[j]);
    }
    scr_bold(0);
    scr_at(0, 49);
    key_label("?", "HELP", BG_BAR);
    scr_spaces(2);
    key_label("TAB", "HINTS", BG_BAR);
    scr_spaces(2);
    key_label("^S", "SOUND", BG_BAR);
    display_link();
    scr_at(1, COL_BOARD);
    scr_ink(C_DIM, C_NONE);
    for (j = 0; j < RULE_COLS; ++j) {
        scr_put(' ');
        scr_put((char)('A' + j));
        scr_put(' ');
    }
    for (i = 0; i < RULE_ROWS; ++i) {
        scr_at((unsigned char)(ROW_BOARD + i), COL_LABEL);
        scr_bold(0);
        scr_ink(C_DIM, C_NONE);
        scr_put((char)('0' + i));
        scr_spaces(COL_BOARD - COL_LABEL - 1);
        for (j = 0; j < RULE_COLS; ++j)
            node_cell((unsigned char)(i * RULE_COLS + j));
    }
    scr_bold(0);
    last_label();
    hives_unknown();
    display_hives();
    scr_at(ROW_BOARD + NSYN + 1, COL_PANEL);
    scr_ink(C_DIM, C_NONE);
    scr_text(T_SAFE " AT ");
    scr_ink(C_TEXT, C_NONE);
    number(RULE_SAFESIZE, 1, 0);
    scr_ink(C_DIM, C_NONE);
    scr_text(", " T_NULLROUTE " AT ");
    scr_ink(C_TEXT, C_NONE);
    number(RULE_ENDSIZE, 1, 0);
    scr_at(ROW_SEATS - 1, COL_NAME_S);
    scr_bold(1);
    scr_ink(C_NEON, C_NONE);
    scr_text(T_PLAYER_P);
    scr_bold(0);
    scr_at(ROW_SEATS - 1, COL_CASH + 10 - (sizeof T_ACCOUNT - 1));
    scr_ink(C_DIM, C_NONE);
    scr_text(T_ACCOUNT);
    for (j = 0; j < NSYN; ++j) {
        scr_at(ROW_SEATS - 1, (unsigned char)(COL_COINS + 5 * j));
        chip(j, C_NONE);
    }
    for (i = 0; i < SEATS; ++i)
        display_seat(i);
    for (i = 0; i < LOG_ROWS; ++i)
        log_row(i);
    display_prompt();
}
#endif

/* ---------------------------------------------------------------- instructions
 * SYS/synack.hlp: the pages one after another, 24 lines each, as made by
 * the Makefile from help/HELP*.TXT.  Page n is lines (n - 1) * 24 on.  This
 * code knows nothing about how many pages there are: '>' past the last
 * finds no page there and stays put.  They open again where they were left. */

#define PAGE_LINES 24
#define PAGE_COL   20

static unsigned char page = 1;

#ifdef _CMOC_VERSION_
#define HELP_FILE "/DD/SYS/synack.hlp"

static unsigned char hpath;

static unsigned char help_openf(void)
{
    static char name[] = HELP_FILE "\r";
    char *nm = name;
    unsigned char p = 0xFF;
    asm {
        lda     #1              ; READ.
        ldx     :nm
        os9     $84             ; I$Open
        bcs     @no
        sta     :p
@no
    }
    hpath = p;
    return p != 0xFF;
}

/* The next line, without its CR; 0 at the end of the file. */
static unsigned char help_line(char *buf, unsigned char size)
{
    unsigned char p = hpath;
    unsigned got = 0;
    asm {
        pshs    y
        ldb     :size
        clra
        tfr     d,y             ; Y: the most to read
        lda     :p              ; A: the path, after D is done with
        ldx     :buf
        os9     $8B             ; I$ReadLn
        bcs     @end
        sty     :got
@end
        puls    y
    }
    if (!got)
        return 0;
    if (buf[got - 1] == '\r')
        --got;
    buf[got] = 0;
    return 1;
}

static void help_closef(void)
{
    unsigned char p = hpath;
    asm {
        lda     :p
        os9     $8F             ; I$Close
    }
}

#else // Unix, for testing: $SYNACK_HLP, else synack.hlp here
#define HELP_FILE "synack.hlp"

#include <stdio.h>
#include <stdlib.h>
static FILE *hf;

static unsigned char help_openf(void)
{
    const char *f = getenv("SYNACK_HLP");
    hf = fopen(f ? f : "synack.hlp", "r");
    return hf != 0;
}

static unsigned char help_line(char *buf, unsigned char size)
{
    size_t k;
    if (!fgets(buf, size, hf))
        return 0;
    k = strlen(buf);
    while (k && (buf[k - 1] == '\n' || buf[k - 1] == '\r'))
        buf[--k] = 0;
    return 1;
}

static void help_closef(void)
{
    fclose(hf);
}
#endif

/* One line of a page: { } highlight, \ is the yen. */
static void help_row(unsigned char r, const char *s)
{
    unsigned char col = 0, mark = 0;
    scr_at(r, PAGE_COL - 1);
    scr_ink(C_NEON, C_NONE);
    scr_glyph("\xE2\x96\x95");          /* the frame: right and left eighths */
    scr_ink(C_TEXT, BG_HELP);
    for (; *s && col < 40; ++s) {
        if (*s == '{' || *s == '}') {
            mark = *s == '{';
            scr_bold(mark);
            scr_ink(mark ? C_WHITE : C_TEXT, mark ? BG_MARK : BG_HELP);
            continue;
        }
        scr_put(*s);
        ++col;
    }
    scr_bold(0);
    scr_ink(C_TEXT, BG_HELP);
    scr_spaces((unsigned char)(40 - col));
    scr_ink(C_NEON, C_NONE);
    scr_glyph("\xE2\x96\x8F");
}

/* Page `p` onto the screen; 0 if there is no such page. */
static unsigned char help_page(unsigned char p)
{
    char line[64];
    unsigned skip = (unsigned)(p - 1) * PAGE_LINES;
    unsigned char r;
    if (!help_openf()) {
        if (p > 1)
            return 0;
        for (r = 0; r < PAGE_LINES; ++r)
            help_row(r, r == 11 ? "  NO INSTRUCTIONS: " HELP_FILE
                        : r == 13 ? "  ESC RETURNS TO THE GAME" : "");
        scr_flush();
        return 1;
    }
    while (skip && help_line(line, sizeof line))
        --skip;
    if (skip || !help_line(line, sizeof line)) {
        help_closef();
        return 0;                       /* past the last page */
    }
    for (r = 0; r < PAGE_LINES; ++r) {
        help_row(r, line);
        if (!help_line(line, sizeof line))
            line[0] = 0;
    }
    help_closef();
    scr_flush();
    return 1;
}

void help_open(void)
{
    setup();
    helping = 1;
    help_page(page);
}

unsigned char help_key(unsigned char k)
{
    switch (k) {
    case '>':
        if (help_page((unsigned char)(page + 1)))
            ++page;
        return 1;
    case '<':
        if (page > 1 && help_page((unsigned char)(page - 1)))
            --page;
        return 1;
    case K_ESC:
    case K_RETURN:
    case K_HELP:
        helping = 0;
        if (field)
            display_playfield();
        else
            display_title_screen();
        return 0;
    }
    return 1;
}
