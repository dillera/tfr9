/* The client's main loop: title, connect, then frames in, keys out, forever.
 * The title screen dials at once and asks for the table's status (`I`), which
 * the server keeps up to date there; Return joins on the same connection. A
 * full table sends the player back to the title screen, still connected.
 * A dropped link reconnects with the seat token; the server resends everything.
 * Esc, then Y, logs off for good: back to the title, a bot in the seat.
 * The server sends a keepalive every 10 s, so silence means a dead link even
 * when the FujiNet still thinks it's connected: NO SIGNAL, then redial. */
#include <string.h>
#include "../config.h"
#include "../platform.h"
#include "../display.h"
#include "proto.h"
#include "proto_tab.h"
#include "game.h"

unsigned char token[4], leaving;
static unsigned char handle[8], handle_len;

static unsigned char c, e, t0, secs, q0, quiet, helping;
static unsigned char tps;                /* frames a second: 60, or 50 on a PAL machine */

#define NO_SIGNAL   20                  /* seconds without a frame: say so */
#define REDIAL      30                  /* ...and hang up and dial again */

#ifdef AUTOPLAY
unsigned char auto_key(void);           /* test/autoplay.c */
#define in_key auto_key
#endif

static void wait_ticks(unsigned char n)
{
    t0 = ticks();
    while ((unsigned char)(ticks() - t0) < n)
        ;
}

/* The next key, or 0. HELP opens the instructions, on the title or mid-game, and
 * while they're up the keys are theirs; the loop goes on reading the network. */
static unsigned char key(void)
{
    c = in_key();
    if (helping) {
        if (c)
            helping = help_key(c);
        return 0;
    }
    if (c == K_HELP) {
        help_open();
        helping = 1;
        return 0;
    }
    return c;
}

/* Dial the server the lobby picked, else the one built in. */
static unsigned char dial(void)
{
#if LOBBY_KEY
    if (net_appkey(LOBBY_KEY)) {
        rx_ring[0] = 'N';               /* over the length: "N:TCP://host:port/" */
        rx_ring[1] = ':';
        return net_open((const char *)rx_ring);
    }
#endif
    return net_open(URL);
}

/* Title: type a handle, Return to jack in. While the player types, the status
 * line shows what joining would do (or NO CARRIER: Return dials again). Only the
 * first draw clears the screen; after that its lines are drawn over, so they
 * don't flicker. */
static void title(void)
{
    if (!link_up)
        status_len = status_code = 0;
    display_title_screen();
    if (!link_up) {                     /* dial while it shows */
        if (dial()) {
            net_close();
            status_code = NO_CARRIER;
        } else {
            link_up = 1;
            tx[0] = 'I';
            tx[1] = PROTO_VERSION;
            tx[2] = TABLE;
            net_send(1 + CLEN_I);
        }
        display_title_update();
    }
    for (;;) {
        if (link_up) {
            if (net_poll()) {
                link_up = status_len = 0;
                status_code = NO_CARRIER;
                net_close();
                display_title_update();
            }
            while ((c = proto_next()) != 0) {   /* only `a` and `i` come before joining */
                game_frame();
                display_title_update();
            }
        }
#if TITLE_IDLE
        display_title_idle();
#endif
        c = key();
        if (c == K_RETURN && edit_len) {
            refused = status_code != ST_FULL;   /* REFUSED_LATE, should it turn out full */
            return;
        }
#if QUIT_TO_OS
        if (c == K_ESC) {               /* Esc, then Y: back to the operating system */
            display_message(QUIT_ASK);
            while ((c = key()) == 0)
                ;
            if (c == 'Y') {
                if (link_up)
                    net_close();
                platform_quit();
            }
            display_title_screen();     /* any other key: the title as it was */
            continue;
        }
#endif
        if (c == K_OPTION)
            hints ^= 1;
        else if (c == K_DELETE && edit_len)
            --edit_len;
        else if (c > ' ' && c <= 'Z' && edit_len < 8)
            edit[edit_len++] = c;
        else
            continue;
        display_title_update();
    }
}

static void hello(void)
{
    tx[0] = 'H';
    tx[1] = PROTO_VERSION;
    tx[2] = TABLE;
    for (c = 0; c < 4; ++c)
        tx[3 + c] = token[c];
    for (c = 0; c < 8; ++c)
        tx[7 + c] = handle[c];
    net_send(1 + CLEN_H);
}

static void link(unsigned char up)
{
    link_up = up;
    display_link();
}

/* Join: on the title screen's connection if it has one, else dial until it answers. */
static void connect(void)
{
    if (!link_up) {
        display_message("DIALING...");
        while ((e = dial()) != 0) {
            display_message("NO CARRIER. REDIALING...");
            net_close();
            wait_ticks(2 * tps);
        }
    }
    hello();
    quiet = 0;
    q0 = ticks();
}

/* Esc, then Y: leave the game for good. The server hands the seat to a bot and
 * forgets the token, so this client goes back to the title as a newcomer. */
static void log_off(void)
{
    tx[0] = 'Q';
    net_send(1 + CLEN_Q);
    net_close();
    for (c = 0; c < 4; ++c)
        token[c] = 0;
    link_up = leaving = refused = 0;
    memcpy(edit, handle, 8);            /* the prompts used it: the handle again */
    edit_len = handle_len;
}

static void redial(const char *why)
{
    link(0);                            /* also tells connect() to dial */
    display_message(why);
    net_close();
    wait_ticks(2 * tps);
    connect();
}

/* The game, from joining on. Returns if the table turned out to be full. */
static void play(void)
{
    for (c = 0; c < 8; ++c)
        handle[c] = c < edit_len ? edit[c] : ' ';
    handle_len = edit_len;
    connect();
    for (;;) {
        if ((e = net_poll()) != 0) {
            redial("CARRIER LOST. REDIALING...");
            continue;
        }
        while ((c = proto_next()) != 0) {
            quiet = 0;
            q0 = ticks();
            if (!link_up)
                link(1);
            if (c == PROTO_BAD) {       /* lost our place in the stream */
                tx[0] = 'R';
                net_send(1);
                break;
            }
            if (c == 'f') {
                if (msg[1] == F_TOKEN) {        /* the server forgot us: join as new */
                    for (e = 0; e < 4; ++e)
                        token[e] = 0;
                    link_up = 0;
                    net_close();
                    connect();
                    break;
                }
                if (msg[1] == F_HANDLE)
                    refused = REFUSED_HANDLE;
                if (msg[1] == F_FULL || msg[1] == F_HANDLE)
                    return;                     /* the server keeps us on, watching again */
                if (msg[1] == F_VERSION) {
                    display_message("SERVER SPEAKS ANOTHER VERSION");
                    for (;;)
                        ;
                }
                continue;
            }
            game_frame();
            if (c == 'p') {
                secs = pr[0];
                t0 = ticks();
            }
        }
        if ((unsigned char)(ticks() - q0) >= tps) {
            q0 += tps;
            ++quiet;
            if (quiet == NO_SIGNAL)
                link(0);
            else if (quiet == REDIAL) {
                redial("NO SIGNAL. REDIALING...");
                continue;
            }
        }
#if DISPLAY_IDLE
        display_idle();
#endif
        c = key();                      /* always read, so no stale key answers the next prompt */
        if (c == K_OPTION) {
            game_toggle_hints();
            c = 0;
        }
        if (leaving && c) {             /* the Y/N after Esc: Y leaves, any other key stays */
            if (c == 'Y') {
                log_off();
                return;
            }
            leaving = 0;
            display_prompt();
            c = 0;
        } else if (c == K_ESC) {
            leaving = 1;
            display_prompt();
            c = 0;
        }
        if (!pr_kind)
            continue;
        if (c)
            game_key(c);
        if (secs && (unsigned char)(ticks() - t0) >= tps) {
            t0 += tps;
            display_clock(--secs);
            if (!secs)
                game_clear_prompt();    /* time's up: the seat's bot answers */
        }
    }
}

void main(void)
{
    tps = ticks_per_sec();
    snd_init();
#if LOBBY_KEY
    net_appkey(0);                      /* the player's lobby name starts the handle */
    for (e = 2; (c = rx_ring[e]) != 0 && edit_len < 8; ++e) {
        if (c >= 'a')
            c -= 'a' - 'A';
        if (c > ' ' && c <= 'Z') {      /* what the title lets a player type */
            edit[edit_len] = c;
            ++edit_len;
        }
    }
#endif
    for (;;) {
        title();
        play();
    }
}
