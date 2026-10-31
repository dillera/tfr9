/* Game state and the rules of the conversation: apply server frames, turn keys
 * into prompt answers. Knows nothing about the screen except display.h.
 *
 * Written for cc65: unsigned char globals, one operation per statement. The
 * client checks answers so the player gets an immediate reason, but the server
 * decides; an answer it refuses comes back as a log line and the same prompt. */
#include <string.h>
#include "../config.h"
#include "../platform.h"
#include "../display.h"
#include "proto.h"
#include "proto_tab.h"
#include "game.h"

const char syn_letter[NSYN + 1] = SYN_LETTERS;
const char *const syn_name[NSYN] = SYN_NAMES;

unsigned char board[NODES];
unsigned char hive_size[NSYN];
unsigned cash[SEATS];
unsigned char coins[SEATS][NSYN];
unsigned char seat_kind[SEATS];
char seat_name[SEATS][8];
unsigned char me, turn = NONE, last_node = NONE, winners;
unsigned char kit[RULE_KITSIZE] = { NONE, NONE, NONE, NONE, NONE, NONE };
unsigned char region[12], region_len;
unsigned char hints = HINTS;
unsigned char link_up;
unsigned char status[30], refused;

unsigned char pr_kind, pr[18];
unsigned char step, edit[9], edit_len;
const char *complaint;

extern unsigned char token[4];          /* main.c */

static unsigned char i, j, b, n, v;

/* ---------------------------------------------------------------- frames in */

static void unpack_snapshot(void)
{
    /* msg[1..54]: board, two nodes per byte, even node in the low nibble */
    j = 0;
    for (i = 1; i <= NODES / 2; ++i) {
        b = msg[i];
        board[j] = b & 15;
        ++j;
        board[j] = b >> 4;
        ++j;
    }
    for (i = 0; i < NSYN; ++i)
        hive_size[i] = msg[55 + i];
    for (i = 0; i < SEATS; ++i)
        cash[i] = msg[62 + 2 * i] | ((unsigned)msg[63 + 2 * i] << 8);
    for (i = 0; i < SEATS; ++i)
        for (j = 0; j < NSYN; ++j)
            coins[i][j] = msg[74 + i * NSYN + j];
    turn = msg[116];
    last_node = NONE;
    if (turn == NONE)                   /* lobby: no kit yet */
        for (i = 0; i < RULE_KITSIZE; ++i)
            kit[i] = NONE;
}

static unsigned char old;

/* Redraw the nodes the kit points at (their hint numbers). */
static void redraw_kit(void)
{
    for (j = 0; j < RULE_KITSIZE; ++j) {
        old = kit[j];
        if (old != NONE)
            display_node(old & 0x7F);
    }
}

void game_toggle_hints(void)
{
    hints ^= 1;
    redraw_kit();
}

static void set_last(unsigned char node)
{
    old = last_node;
    last_node = node;
    if (old != NONE)
        display_node(old);
}

void game_clear_prompt(void)
{
    b = pr_kind;
    pr_kind = 0;
    complaint = 0;
    display_prompt();
    if (b == 'B')
        display_hives();                /* back from prices to names */
}

void game_frame(void)
{
    switch (msg[0]) {
    case 'w':                           /* welcome: seat, token */
        me = msg[1];
        for (i = 0; i < 4; ++i)
            token[i] = msg[2 + i];
        break;
    case 'a':                           /* region name */
        n = msg[1];
        if (n > sizeof region)
            n = sizeof region;
        region_len = n;
        for (i = 0; i < n; ++i)
            region[i] = msg[2 + i];
        break;
    case 'i':                           /* title screen status: code, text (<= 28) */
        memcpy(status, msg + 1, sizeof status);
        break;
    case 'n':                           /* seat, kind, handle[8] */
        n = msg[1];
        seat_kind[n] = msg[2];
        for (i = 0; i < 8; ++i)
            seat_name[n][i] = msg[3 + i];
        display_seat(n);
        break;
    case 's':
        unpack_snapshot();
        pr_kind = 0;
        display_playfield();
        break;
    case 'h':                           /* kit */
        for (i = 0; i < RULE_KITSIZE; ++i) {
            b = kit[i];
            kit[i] = msg[1 + i];
            if (b != NONE)              /* its old node loses its mark */
                display_node(b & 0x7F);
        }
        redraw_kit();
        if (!pr_kind)
            display_prompt();
        break;
    case 't':                           /* whose turn */
        n = turn;
        turn = msg[1];
        if (n < SEATS)
            display_seat(n);
        display_seat(turn);
        if (turn != me && pr_kind && pr_kind != 'S')
            game_clear_prompt();             /* our bot answered for us */
        else if (!pr_kind)
            display_prompt();                /* WAITING FOR, or clear it on our turn */
        break;
    case 'x':                           /* len, val, nodes... */
        v = msg[2];
        n = msg[1] + 1;
        if (v == CELL_SPIKE && n == 3) {    /* a single new spike: the move just made */
            set_last(msg[3]);
            play_chord(CHORD_SPIKE);
        }
        for (i = 3; i <= n; ++i) {
            b = msg[i];
            board[b] = v;
            display_node(b);
        }
        break;
    case 'r':                           /* hivejack: every `from` becomes `to` */
        play_chord(CHORD_JACK);
        v = msg[1];
        b = msg[2];
        for (i = 0; i < NODES; ++i)
            if (board[i] == v) {
                board[i] = b;
                display_node(i);
            }
        break;
    case 'z':                           /* syn, size */
        n = msg[1];
        b = hive_size[n];
        v = msg[2];
        hive_size[n] = v;
        if (b < RULE_SAFESIZE && v >= RULE_SAFESIZE)
            play_chord(CHORD_ICE);
        else if (!b && v)
            play_chord(CHORD_HIVE);
        if (pr_kind != 'B')
            display_hives();
        break;
    case 'm':                           /* seat, cash */
        n = msg[1];
        cash[n] = msg[2] | ((unsigned)msg[3] << 8);
        display_seat(n);
        break;
    case 'k':                           /* seat, syn, count */
        n = msg[1];
        coins[n][msg[2]] = msg[3];
        display_seat(n);
        break;
    case 'p':                           /* len, kind, timeout, params */
        pr_kind = msg[2];
        n = msg[1];
        for (i = 3; i <= n + 1 && i < 3 + sizeof pr; ++i)
            pr[i - 3] = msg[i];         /* pr[0] = timeout, pr[1..] = params */
        step = 0;
        edit_len = 0;
        complaint = 0;
        if (pr_kind == 'B') {
            if (pr[3] & 2)              /* nothing to buy: asked only because */
                step = 1;               /* the game may end */
            display_hives();            /* names become prices */
            play_chord(CHORD_BUY);
        } else if (pr_kind == 'P')
            play_chord(CHORD_TURN);
        display_prompt();
        display_clock(pr[0]);
        break;
    case 'l':
        display_log();
        break;
    case 'e':
        winners = msg[1];
        n = turn;
        turn = NONE;                    /* back in the lobby */
        if (n < SEATS) {                /* not the replay of an old result */
            display_seat(n);
            play_chord(CHORD_END);
        }
        game_clear_prompt();
        display_game_over();
        break;
    }
}

/* ---------------------------------------------------------------- keys out */

static void send2(unsigned char a)
{
    tx[0] = pr_kind;
    tx[1] = a;
    net_send(2);
    game_clear_prompt();
}

static void refuse(const char *why)
{
    complaint = why;
    display_prompt();
}

/* Letter -> syndicate id, or NONE. */
static unsigned char syn_of(unsigned char c)
{
    for (v = 0; v < NSYN; ++v)
        if (syn_letter[v] == c)
            return v;
    return NONE;
}

/* Why an exploit can't run (the server only marks it): next to two or more
 * hives it would join two Black ICE hives; otherwise it would found a hive
 * with every syndicate already on the Grid. */
static unsigned char seen;

static void look(unsigned char node)
{
    v = board[node];
    if (v >= 2)
        seen |= 1 << (v - 2);
}

static const char *why_not(unsigned char node)
{
    seen = 0;
    n = node;
    for (b = 0; n >= RULE_COLS; ++b)    /* b = row, n = column */
        n -= RULE_COLS;
    if (b)
        look(node - RULE_COLS);
    if (b < RULE_ROWS - 1)
        look(node + RULE_COLS);
    if (n)
        look(node - 1);
    if (n < RULE_COLS - 1)
        look(node + 1);
    if (seen & (seen - 1))              /* two or more syndicates */
        return SAY_JOINS;
    return SAY_NOSYN;
}

/* The number typed so far (0 if nothing). */
static unsigned char typed(void)
{
    v = 0;
    for (j = 0; j < edit_len; ++j)
        v = v * 10 + edit[j] - '0';
    return v;
}

static unsigned char sell;

static void key_dispose(unsigned char c)
{
    /* pr: timeout, absorbed, absorber, held, supply */
    if (c >= '0' && c <= '9' && edit_len < 2) {
        edit[edit_len++] = c;
    } else if (c == K_DELETE && edit_len) {
        --edit_len;
    } else if (c == K_RETURN) {
        n = typed();
        if (step == 0) {
            if (n > pr[3])
                { refuse("YOU DON'T HOLD THAT MANY"); return; }
            sell = n;
            step = 1;
        } else if (n & 1) {
            { refuse(SAY_EVEN); return; }
        } else if (sell + n > pr[3]) {
            { refuse("YOU DON'T HOLD THAT MANY"); return; }
        } else if (n / 2 > pr[4]) {
            { refuse(SAY_POOL); return; }
        } else {
            tx[0] = 'D';
            tx[1] = sell;
            tx[2] = n;
            net_send(3);
            game_clear_prompt();
            return;
        }
        edit_len = 0;
    } else {
        return;
    }
    complaint = 0;
    display_prompt();
}

static void send_buy(unsigned char end)
{
    tx[0] = 'B';
    for (i = 0; i < 3; ++i)
        tx[1 + i] = i < edit_len ? syn_of(edit[i]) : 7;
    if (end)
        tx[1] |= 0x80;
    net_send(4);
    game_clear_prompt();
}

/* Money left after the letters typed so far; pr: timeout, cash lo, cash hi,
 * flags, price[7], supply[7]. */
static unsigned left_cash(void)
{
    unsigned m = pr[1] | ((unsigned)pr[2] << 8);
    for (j = 0; j < edit_len; ++j)
        m -= pr[4 + syn_of(edit[j])];
    return m;
}

static void key_buy(unsigned char c)
{
    if (step == 1) {                    /* null-route? */
        if (c == 'Y')
            send_buy(1);
        else if (c == 'N' || c == K_RETURN)
            send_buy(0);
        return;
    }
    if (c == K_RETURN) {
        if (pr[3] & 1) {
            step = 1;
            complaint = 0;
            display_prompt();
        } else {
            send_buy(0);
        }
        return;
    }
    if (c == K_DELETE) {
        if (edit_len)
            --edit_len;
        complaint = 0;
        display_prompt();
        return;
    }
    b = syn_of(c);
    if (b == NONE)
        return;
    if (edit_len == RULE_MAXBUY)
        { refuse(SAY_MAXBUY); return; }
    if (!pr[4 + b])
        { refuse("NO HIVE ON THE GRID"); return; }
    n = 0;
    for (j = 0; j < edit_len; ++j)
        if (edit[j] == c)
            ++n;
    if (n >= pr[4 + NSYN + b])
        { refuse("NONE LEFT IN THE " T_BANK); return; }
    if (left_cash() < pr[4 + b])
        { refuse("NOT ENOUGH " T_MONEY); return; }
    edit[edit_len++] = c;
    complaint = 0;
    display_prompt();
}

void game_key(unsigned char c)
{
    switch (pr_kind) {
    case 'S':                           /* lobby host: 2-6 seats */
        if (c >= '0' + RULE_MINPLAYERS && c <= '0' + RULE_MAXPLAYERS)
            send2(c - '0');
        break;
    case 'P':                           /* 1-6: run that exploit */
        n = c - '1';
        if (n >= RULE_KITSIZE)
            break;
        b = kit[n];
        if (b == NONE)
            refuse("THAT SLOT IS EMPTY");
        else if (b & 0x80)
            refuse(why_not(b & 0x7F));
        else
            send2(b);
        break;
    case 'F':                           /* broker: pr[1] = available mask */
        b = syn_of(c);
        if (b != NONE && (pr[1] >> b & 1))
            send2(b);
        break;
    case 'K':                           /* tie: pr[2] = options mask */
        b = syn_of(c);
        if (b != NONE && (pr[2] >> b & 1))
            send2(b);
        break;
    case 'D':
        key_dispose(c);
        break;
    case 'B':
        key_buy(c);
        break;
    }
}
