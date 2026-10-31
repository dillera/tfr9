/* Everything the game shows goes through these calls. Game logic (common/) never
 * draws. Each platform has a text version and may have a fancy one;
 * config.h picks per screen. The display reads the game state in game.h. */
#ifndef DISPLAY_H
#define DISPLAY_H

/* FANCY_TITLE */
void display_title_screen(void);        /* title, region, the handle typed so far (edit[]) */
void display_title_update(void);        /* just its changing lines, drawn over in place */

/* the instruction pages, from the title screen; reads keys with `key` until Esc */
void help_open(void);                   /* the instructions over the screen, which stays as it was */
unsigned char __fastcall__ help_key(unsigned char k);  /* a key for them: 0 = they closed */

/* FANCY_PLAYFIELD */
void display_playfield(void);           /* full redraw from the game state */
void display_node(unsigned char node);  /* one Grid node changed */
void display_hives(void);               /* hive sizes; buy prices while prompt B is open */
void display_seat(unsigned char seat);  /* one seat's name, cash and crypto */
void display_log(void);                 /* msg[] holds an 'l' frame: show its text */
void display_prompt(void);              /* the open prompt (or the kit when none) */
void display_clock(unsigned char secs); /* seconds left to answer */
void display_message(const char *s);    /* one-line notice: connecting, faults... */
void display_game_over(void);           /* winners are in `winners` */
void display_link(void);                /* region and link_up, in the title bar */
void display_idle(void);                /* every pass of the game loop, if DISPLAY_IDLE */
void display_title_idle(void);          /* every pass of the title's loop, if TITLE_IDLE */

#endif
