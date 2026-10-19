/**
 * @brief   fnweather: the cache, a file of the last Report
 * @license gpl v. 3
 * @verbose The place, the units, and the last weather fetched, with when
 *          it was fetched, so that fnweather shows them at once, and
 *          fetches again only when they are STALE_MINUTES old.
 */

#include "weather.h"

#ifdef _CMOC_VERSION_

const char cache_path[] = "/DD/SYS/fnweather.dat";
static char os9_path[] = "/DD/SYS/fnweather.dat\r";

// I$Open (READ.) or I$Create (WRITE., attributes PR R W): the path, or 255.
static byte os9_open(byte create)
{
    char *name = os9_path;
    byte path = 255;
    asm {
        ldx     :name
        tst     :create
        bne     @create
        lda     #1              ; READ.
        os9     $84             ; I$Open
        bra     @done
@create
        lda     #2              ; WRITE.
        ldb     #$0B            ; PREAD. READ. WRITE.
        os9     $83             ; I$Create
@done
        bcs     @fail
        sta     :path
@fail
    }
    return path;
}

static void os9_delete(void)
{
    char *name = os9_path;
    asm {
        ldx     :name
        lda     #2
        os9     $87             ; I$Delete
    }
}

// I$Read or I$Write of the Report: the count moved.
static word os9_move(byte path, byte write)
{
    Report *r = &rep;
    word n = sizeof rep, got = 0;
    asm {
        lda     :path
        ldx     :r
        pshs    y
        ldy     :n
        tst     :write
        bne     @write
        os9     $89             ; I$Read
        bra     @done
@write
        os9     $8A             ; I$Write
@done
        bcs     @fail
        sty     :got
@fail
        puls    y
    }
    return got;
}

static void os9_close(byte path)
{
    asm {
        lda     :path
        os9     $8F             ; I$Close
    }
}

byte cache_load(void)
{
    byte path = os9_open(0), ok;
    if (path == 255) return 0;
    ok = os9_move(path, 0) == sizeof rep && !strcmp(rep.magic, CACHE_MAGIC);
    os9_close(path);
    if (!ok) memset(&rep, 0, sizeof rep);
    return ok;
}

void cache_save(void)
{
    byte path;
    strcpy(rep.magic, CACHE_MAGIC);
    os9_delete();
    path = os9_open(1);
    if (path == 255) return;
    os9_move(path, 1);
    os9_close(path);
}

#else // Unix, for testing: $FNWEATHER_CACHE, or ./fnweather.dat

const char cache_path[] = "fnweather.dat";

static const char *unix_path(void)
{
    const char *p = getenv("FNWEATHER_CACHE");
    return p ? p : cache_path;
}

byte cache_load(void)
{
    FILE *f = fopen(unix_path(), "rb");
    byte ok;
    if (!f) return 0;
    ok = fread(&rep, sizeof rep, 1, f) == 1 && !strcmp(rep.magic, CACHE_MAGIC);
    fclose(f);
    if (!ok) memset(&rep, 0, sizeof rep);
    return ok;
}

void cache_save(void)
{
    FILE *f = fopen(unix_path(), "wb");
    strcpy(rep.magic, CACHE_MAGIC);
    if (!f) return;
    fwrite(&rep, sizeof rep, 1, f);
    fclose(f);
}

#endif
