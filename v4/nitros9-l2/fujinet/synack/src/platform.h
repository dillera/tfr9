/* The contract between common code and each platform (src/<platform>/).
 * Keep it tiny: every function here is written once per machine. */
#ifndef PLATFORM_H
#define PLATFORM_H

/* ------------------------------------------------------------------ network
 * One TCP connection to the server. Received bytes land in rx_ring, a 256-byte
 * ring: net_poll() advances rx_tail, the frame assembler (proto.c) advances
 * rx_head. Both are bytes, so they wrap for free. The ring never holds more
 * than 255 bytes, so head == tail always means empty.
 *
 * Return codes: 0 = fine. Anything else means the link is down: the platform's
 * own error code (Atari: SIO status 128-255), or NET_CLOSED. */
#define NET_CLOSED 0xFF

extern unsigned char rx_ring[256];
extern unsigned char rx_head, rx_tail;
extern unsigned char tx[16];            /* the next client frame, built in place */

unsigned char __fastcall__ net_open(const char *url);   /* "N:TCP://host:port/" */
unsigned char net_poll(void);           /* move waiting bytes into rx_ring */
unsigned char __fastcall__ net_send(unsigned char len); /* send tx[0..len-1] */
void net_close(void);

/* The FujiNet game lobby's AppKey `key` (creator 1, app 1), read into rx_ring
 * as a string from rx_ring[2] on: returns its length (1-64), or 0 (an empty
 * string) if there is none. Key 0 is the player's lobby name. The ring must be
 * idle: call it only before net_open. */
unsigned char __fastcall__ net_appkey(unsigned char key);

/* ------------------------------------------------------------------ input, time */
#define K_RETURN '\r'
#define K_DELETE '\b'
#define K_OPTION 0x01                   /* toggles move hints (Atari: the OPTION key) */
#define K_HELP   0x02                   /* instructions (Atari: the HELP key, XL/XE) */
#define K_ESC    0x1B
unsigned char in_key(void);             /* next key as uppercase ASCII, or 0 if none */
unsigned char ticks(void);              /* free-running 8-bit frame counter */
unsigned char ticks_per_sec(void);      /* how fast it runs: 60 a second, 50 on a PAL machine */

/* ------------------------------------------------------------------ sound
 * A short chord for a board event. A chord that is still sounding is only cut
 * off by a higher-numbered one. */
#define CHORD_SPIKE 0                   /* any move: a short low chord */
#define CHORD_BUY   1                   /* your buy: two rising chords */
#define CHORD_HIVE  2                   /* a new hive */
#define CHORD_JACK  3                   /* a hivejack */
#define CHORD_ICE   4                   /* a hive becomes Black ICE */
#define CHORD_TURN  5                   /* your turn: two rising chords */
#define CHORD_END   6                   /* the null-route: a longer series */

void snd_init(void);
void __fastcall__ play_chord(unsigned char n);
void snd_toggle(void);                  /* sound on/off; on plays CHORD_HIVE */

#if QUIT_TO_OS
void platform_quit(void);               /* the machine as it was found, and the program ends */
#endif

#endif
