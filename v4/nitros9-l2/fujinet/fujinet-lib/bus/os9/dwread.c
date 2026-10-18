/**
 * @brief Read string to s from DriveWire with expected length l
 * @param s pointer to string buffer
 * @param l expected length of string (0-512 bytes under NitrOS-9)
 * @return 1 = read successful, 0 = not successful
 *
 * NitrOS-9: sends what dwwrite() queued, then reads the reply, in one
 * SS.Fuji call (see ssfuji.c).
 */

#include "dw.h"
#include "ssfuji.h"

byte dwread(byte *s, int l)
{
    return ssfuji_read(s, (word) l);
}
