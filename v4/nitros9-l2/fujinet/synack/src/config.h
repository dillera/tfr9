/* Build-time choices. The Makefile can override any of these (make FANCY=1 URL=...). */
#ifndef CONFIG_H
#define CONFIG_H

/* 0 = the text-only screen, 1 = the hand-made one. Each screen switches
 * on its own, so a fancy title can arrive before a fancy playfield. */
#ifndef FANCY_TITLE
#define FANCY_TITLE     0
#endif
#ifndef FANCY_PLAYFIELD
#define FANCY_PLAYFIELD 0
#endif

/* Show, on each empty node an exploit in your kit can run on, that exploit's
 * key (1-6). The player can flip it with OPTION on the title screen and in the game. */
#ifndef HINTS
#define HINTS 1
#endif

/* The server to dial, unless the FujiNet game lobby picked one: it stores the
 * server's address ("TCP://host:port/") in AppKey creator 1, app 1, key LOBBY_KEY,
 * the number the server registers with the lobby as its `appkey` (1-255).
 * LOBBY_KEY 0: always dial URL and leave the handle empty. */
#ifndef URL
#define URL "N:TCP://syn-ack-wire.fly.dev:6502/"
#endif
#ifndef LOBBY_KEY
#define LOBBY_KEY 11
#endif
#ifndef TABLE
#define TABLE 0
#endif

/* 1 = the game loop calls display_idle() on every pass, for a display that
 * changes with time (apple2_text: the last move flashes for a few seconds). */
#ifndef DISPLAY_IDLE
#define DISPLAY_IDLE 0
#endif

/* 1 = the title's loop calls display_title_idle() on every pass (coco: the
 * static in the logo's letters). */
#ifndef TITLE_IDLE
#define TITLE_IDLE 0
#endif

/* 1 = Esc on the title screen asks QUIT_ASK, and Y calls platform_quit(): for
 * a platform with somewhere to go back to (msdos: DOS). */
#ifndef QUIT_TO_OS
#define QUIT_TO_OS 0
#endif
#ifndef QUIT_ASK
#define QUIT_ASK "QUIT TO DOS? Y/N"
#endif

/* What game.c says when it refuses an answer itself, on the question line.
 * NARROW_SAYS=1 words it for a narrower one (nes: 26 columns beside the clock),
 * and names START, the NES's Return. Any one can also be set on its own (adam:
 * the narrow ones, RETURN in SAY_MAXBUY). */
#if NARROW_SAYS
#ifndef SAY_MAXBUY
#define SAY_MAXBUY  "3 PER TURN: START TO BUY"
#endif
#ifndef SAY_JOINS
#define SAY_JOINS   "JOINS 2 " T_SAFE " HIVES"
#endif
#ifndef SAY_NOSYN
#define SAY_NOSYN   "NO SYNDICATE LEFT"
#endif
#ifndef SAY_EVEN
#define SAY_EVEN    T_SWAP " AN EVEN NUMBER"
#endif
#ifndef SAY_POOL
#define SAY_POOL    "NOT ENOUGH IN THE " T_BANK
#endif
#endif
#ifndef SAY_MAXBUY
#define SAY_MAXBUY  "3 PER TURN: RETURN TO BUY"
#endif
#ifndef SAY_JOINS
#define SAY_JOINS   T_PATCHED ": JOINS 2 " T_SAFE " HIVES"
#endif
#ifndef SAY_NOSYN
#define SAY_NOSYN   "NO SYNDICATE LEFT FOR A NEW " T_HIVE
#endif
#ifndef SAY_EVEN
#define SAY_EVEN    T_SWAP_P " GO 2 FOR 1: EVEN ONLY"
#endif
#ifndef SAY_POOL
#define SAY_POOL    "NOT ENOUGH LEFT IN THE " T_BANK
#endif

#endif
