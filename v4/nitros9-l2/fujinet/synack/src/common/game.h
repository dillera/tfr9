/* Game state, as the server last described it. The display reads it; only
 * game.c writes it. */
#ifndef GAME_H
#define GAME_H

#include "theme_tab.h"

extern const char syn_letter[NSYN + 1];       /* board letters, in syndicate id order */
extern const char *const syn_name[NSYN];      /* names for the screen */

#define NODES (RULE_ROWS * RULE_COLS)
#define SEATS RULE_MAXPLAYERS
#define NONE 0xFF

/* board cell values (protocol): 0 empty, 1 spike, 2.. hive of syndicate (value - 2) */
#define CELL_EMPTY 0
#define CELL_SPIKE 1

extern unsigned char board[NODES];
extern unsigned char hive_size[NSYN];
extern unsigned cash[SEATS];                    /* units of 100 */
extern unsigned char coins[SEATS][NSYN];
extern unsigned char seat_kind[SEATS];          /* SEAT_EMPTY/HUMAN/AWAY/BOT */
extern char seat_name[SEATS][8];                /* space padded */
extern unsigned char me, turn, last_node, winners;
extern unsigned char kit[RULE_KITSIZE];         /* node, bit 7 = can't run now; NONE = empty */
extern unsigned char region[12], region_len;
extern unsigned char hints;                     /* show where the kit can run */
extern unsigned char link_up;                   /* 0 = no frame from the server for a while */
/* the title screen: the server's status line, kept as its `i` frame (length,
 * ST_ code, text), and why joining was last refused */
extern unsigned char status[30], refused;
#define REFUSED_LATE   1                        /* full, though the status showed a seat */
#define REFUSED_HANDLE 2                        /* that handle is taken there */
#define status_len  status[0]                   /* 0 = none; else 1 + the text's length */
#define status_code status[1]
#define status_text (status + 2)
#define NO_CARRIER 0xFF                         /* status_code: the title screen couldn't dial */

/* the open prompt: kind letter (0 = none) and its params, see docs/protocol.md */
extern unsigned char pr_kind, pr[18];
/* answering it: step within the prompt, text typed so far, last complaint */
extern unsigned char step, edit[9], edit_len;
extern const char *complaint;
/* main.c: Esc asked whether to log off; the prompt line shows the Y/N instead */
extern unsigned char leaving;

void game_frame(void);                  /* apply the frame in msg[] */
void game_key(unsigned char c);         /* a key while a prompt is open */
void game_clear_prompt(void);           /* answered, or the clock ran out */
void game_toggle_hints(void);           /* K_OPTION */

#endif
