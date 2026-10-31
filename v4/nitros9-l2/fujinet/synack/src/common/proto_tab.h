/* GENERATED from server/acquire/proto.py by tools/gen_proto.py -- do not edit. */
#ifndef PROTO_TAB_H
#define PROTO_TAB_H

#define PROTO_VERSION 3
#define LEN_VAR 0xFE   /* [type][len][payload] */
#define LEN_BAD 0xFF   /* not a server message type */

/* server -> client payload length, indexed by type - 'a' (defined in proto.c) */
extern const unsigned char len_tab[26];
#define LEN_TAB { \
    /*a*/ LEN_VAR, /*b*/ LEN_BAD, /*c*/ LEN_BAD, /*d*/ LEN_BAD, /*e*/ 1, /*f*/ 1, \
    /*g*/ LEN_BAD, /*h*/ 6, /*i*/ LEN_VAR, /*j*/ LEN_BAD, /*k*/ 3, /*l*/ LEN_VAR, \
    /*m*/ 3, /*n*/ 10, /*o*/ LEN_BAD, /*p*/ LEN_VAR, /*q*/ LEN_BAD, /*r*/ 2, \
    /*s*/ 116, /*t*/ 1, /*u*/ 0, /*v*/ LEN_BAD, /*w*/ 5, /*x*/ LEN_VAR, \
    /*y*/ LEN_BAD, /*z*/ 2, \
}

/* client -> server payload lengths */
#define CLEN_I 2
#define CLEN_H 14
#define CLEN_S 1
#define CLEN_P 1
#define CLEN_F 1
#define CLEN_K 1
#define CLEN_D 2
#define CLEN_B 3
#define CLEN_R 0
#define CLEN_Q 0

/* seat kinds (n), fault codes (f), table status (i) */
#define SEAT_EMPTY 0
#define SEAT_HUMAN 1
#define SEAT_AWAY 2
#define SEAT_BOT 3
#define F_VERSION 1
#define F_FULL 2
#define F_TOKEN 3
#define F_ILLEGAL 4
#define F_HANDLE 5
#define ST_LOBBY 0
#define ST_JACKIN 1
#define ST_FULL 2

#endif
