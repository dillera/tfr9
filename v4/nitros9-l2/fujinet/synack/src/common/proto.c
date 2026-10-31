/* Written for cc65's code generator: unsigned char globals, one operation per
 * statement, no int promotion. Each line should compile to a few instructions. */
#include "../platform.h"
#include "proto.h"
#include "proto_tab.h"

unsigned char msg[128];
const unsigned char len_tab[26] = LEN_TAB;

static unsigned char avail, need, i, b;

unsigned char proto_next(void)
{
    avail = rx_tail;
    avail -= rx_head;
    if (!avail)
        return 0;
    i = rx_head;
    b = rx_ring[i];
    b -= 'a';
    need = LEN_BAD;
    if (b < 26)
        need = len_tab[b];
    if (need == LEN_VAR) {
        if (avail < 2)
            return 0;
        ++i;
        need = rx_ring[i];
        if (need >= sizeof msg - 1)
            need = LEN_BAD;
        else
            ++need;
    }
    if (need == LEN_BAD) {
        rx_head = rx_tail;
        return PROTO_BAD;
    }
    if (avail <= need)          /* type byte + need payload bytes */
        return 0;
    i = 0;
    do {
        b = rx_ring[rx_head];
        ++rx_head;
        msg[i] = b;
        ++i;
    } while (need--);
    return msg[0];
}
