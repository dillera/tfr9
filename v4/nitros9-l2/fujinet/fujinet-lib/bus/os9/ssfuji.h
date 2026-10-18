/**
 * @brief   NitrOS-9 transport: DriveWire exchanges through SS.Fuji
 * @license gpl v. 3, see LICENSE for details.
 * @verbose See ssfuji.c.
 */

#ifndef SSFUJI_H
#define SSFUJI_H

#include <cmoc.h>
#include <coco.h>

/**
 * @brief The most a request or a reply may be (scdwv's FUJI$MaxTran)
 */
#define SSFUJI_MAX 512

/**
 * @brief Queue bytes for the FujiNet; they go with the next read
 * @return 1 = queued, 0 = an earlier exchange failed (see ssfuji_error)
 */
byte ssfuji_write(const byte *s, word l);

/**
 * @brief Send what is queued, then read l bytes of reply into s
 * @return 1 = read, 0 = not (nothing queued, too long, or an OS-9 error)
 */
byte ssfuji_read(byte *s, word l);

/**
 * @brief Send what is queued, wanting no reply
 * @return 1 = sent, 0 = not
 */
byte ssfuji_flush(void);

/**
 * @brief The OS-9 error of the last exchange, or 0
 */
extern byte ssfuji_error;

#endif /* SSFUJI_H */
