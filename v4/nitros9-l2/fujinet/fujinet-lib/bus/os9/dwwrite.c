/**
 * @brief Write string at s to DriveWire with length l
 * @param s pointer to string buffer
 * @param l length of string (0-65535 bytes)
 * @return 1 = queued, 0 = not
 *
 * NitrOS-9: the bytes are queued, and go to the FujiNet with the next
 * dwread(), in one SS.Fuji call (see ssfuji.c).
 */

#include "dw.h"
#include "ssfuji.h"

byte dwwrite(byte *s, int l)
{
    return ssfuji_write(s, (word) l);
}
