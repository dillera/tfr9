/* Frame assembler: turns the byte stream in rx_ring into whole frames in msg[]. */
#ifndef PROTO_H
#define PROTO_H

#define PROTO_BAD '?'           /* not a server type: ring flushed, send R to resync */

/* msg[0] = type letter, then the payload. Variable-length types (a l p x) keep
 * their length byte at msg[1], so their payload starts at msg[2]. */
extern unsigned char msg[128];

/* Copy the next whole frame from rx_ring into msg[] and return its type,
 * or 0 if no whole frame has arrived yet. */
unsigned char proto_next(void);

#endif
