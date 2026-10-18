/**
 * @brief   NitrOS-9 transport: DriveWire exchanges through SS.Fuji
 * @license gpl v. 3, see LICENSE for details.
 * @verbose On a CoCo, dwwrite() and dwread() call the DriveWire ROM.
 *          Under NitrOS-9 the wire belongs to the system: a program
 *          reaches the FujiNet through scdwv's SS.Fuji SetStat on /N1,
 *          which sends a request and reads its reply with interrupts
 *          masked, so that nothing else on the wire can take the reply.
 *
 *          So dwwrite() only queues its bytes, and dwread() sends what is
 *          queued and reads the reply, as one SS.Fuji call.  A request is
 *          always followed by a read in fujinet-lib (at least for the
 *          status of the command), so nothing stays queued.  Should more
 *          than SSFUJI_MAX bytes be queued, the first SSFUJI_MAX go ahead
 *          on their own.
 *
 *          The SS.Fuji parameter block is the reply's length (two bytes,
 *          high byte first), then the request; X points to it, Y is its
 *          length, U is where the reply goes.
 */

#include "ssfuji.h"

#define SS_FUJI 0xE2

static byte fn_path = 255;          /* /N1, opened on first use */
static byte blk[2 + SSFUJI_MAX];    /* the reply's length, then the request */
static word queued;                 /* request bytes at blk + 2 */

byte ssfuji_error;

/* One SS.Fuji call: what is queued, then rlen bytes of reply. */
static byte exchange(void *reply, word rlen)
{
  static char name[] = "/N1\r";
  char *nm = name;
  byte *b = blk;
  byte path = fn_path, err = 0;
  word n = queued + 2;

  queued = 0;
  if (path == 255) {
    asm
    {
        lda     #3              ; UPDAT.
        ldx     :nm
        os9     $84             ; I$Open
        bcs     @bad
        sta     :path
        bra     @done
@bad
        stb     :err
@done
    }
    if (err)
      return err;
    fn_path = path;
  }
  blk[0] = (byte) (rlen >> 8);
  blk[1] = (byte) rlen;
  asm
  {
        lda     :path
        ldx     :b
        pshs    u,y
        ldy     :n
        ldu     :reply          ; last: U is the frame pointer
        ldb     #SS_FUJI
        os9     $8E             ; I$SetStt
        puls    u,y
        bcc     @ok
        stb     :err
@ok
  }
  return err;
}

byte ssfuji_write(const byte *s, word l)
{
  word n;

  while (l) {
    if (queued == SSFUJI_MAX && (ssfuji_error = exchange(NULL, 0)) != 0)
      return 0;
    n = SSFUJI_MAX - queued;
    if (n > l)
      n = l;
    memcpy(blk + 2 + queued, s, n);
    queued += n;
    s += n;
    l -= n;
  }
  return 1;
}

byte ssfuji_read(byte *s, word l)
{
  if (!queued || l > SSFUJI_MAX)
    return 0;
  ssfuji_error = exchange(s, l);
  return ssfuji_error == 0;
}

byte ssfuji_flush(void)
{
  if (!queued)
    return 1;
  ssfuji_error = exchange(NULL, 0);
  return ssfuji_error == 0;
}
