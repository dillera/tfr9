// splash.c -- the boot screen of NitrOS-9 Level 2 on the TFR/911H: "TFR"
// and "FujiNet" in big letters, joined by the band of stripes from the
// manual's cover, and what the system can tell about itself.
//
//   splash
//
// startup runs it, before tsh.  It clears the screen, then writes:
//
//   System    Init's name for the OS; the 6309's speed, from the
//             firmware's info port ($FF43, $FF44); RAM, from F$GBlkMp
//   Booted    /DD, the other name its drive goes by (/H0, the tether's
//             disk; /X0, the FujiNet's), its driver and drive: from the
//             module directory (F$GModDr, F$CpyMem), as info reads it
//   Time      F$Time
//   FujiNet   its address and firmware (fuji_get_adapter_config), if it
//             is there: NitrOS-9 was booted from it, or it answers
//             DriveWire's OP_TIME on the becker port ($FF45, $FF46)
//             within a fifth of a second.  It must not be asked through
//             dwio otherwise: dwio waits for an answer for ever.
//
// The picture is art.h, made by art.py: 256 colours and half blocks in
// UTF-8, which the tether passes to the terminal as they are.
//
// For this computer, to look at:  make build/splash-host  (with made-up
// values for the system and the FujiNet).
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#ifdef _CMOC_VERSION_
#include <fujinet-int.h>
#include <fujinet-fuji.h>
#else
#include <stdio.h>
#include <string.h>
typedef unsigned char byte;
typedef unsigned short word;
#endif

#include "art.h"

// ── The layout (rows and columns from 0) ───────────────────────────
// Beside TFR, the system; under the band, left of where it turns down,
// the FujiNet; under the picture, a hint; then the shell's prompt.
#define SYS_COL   35
#define SYS_WIDTH 45
#define SYS_ROW   1
#define FN_COL    2
#define FN_ROW    12
#define HINT_ROW  (ART_ROWS + 1)
#define END_ROW   (ART_ROWS + 3)

#define LABEL "38;5;172"        // orange, as the cover's
#define DIM   "38;5;245"

// ── Output, collected and written at once ──────────────────────────

static char out[600];
static word outn;

static void put(const char *s)
{
    while (*s && outn < sizeof out)
        out[outn++] = *s++;
}

static void putch(char c)
{
    if (outn < sizeof out)
        out[outn++] = c;
}

static void putu(word n)
{
    char b[6];
    byte i = 0;
    do {
        b[i++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    while (i)
        putch(b[--i]);
}

static void put2(byte n)        // two digits
{
    putch((char)('0' + n / 10));
    putch((char)('0' + n % 10));
}

static void sgr(const char *s)
{
    put("\033[");
    put(s);
    putch('m');
}

static void at(byte row, byte col)
{
    put("\033[");
    putu(row + 1);
    putch(';');
    putu(col + 1);
    putch('H');
}

static void label(byte row, byte col, const char *s)
{
    at(row, col);
    sgr(LABEL);
    put(s);
    sgr("0");
}

// ── What the system knows ──────────────────────────────────────────

static char os_name[48];        // "NitrOS-9/6809 Level 2 V03.3.0"
static char mhz[8];             // "1.79", or empty
static word ram_k, free_k;
static byte dd_found;           // /DD's descriptor:
static char dd_twin[12];        //   the other name of its drive ("H0")
static char dd_driver[12];      //   its driver ("EmuDsk", "rbdw")
static byte dd_drive;           //   and its drive number
static byte from_fujinet;       // booted from it: the driver is rbdw
static byte when[6];            // F$Time: year - 1900, month, day, h, m, s

static byte fn_there;           // the FujiNet answered
static byte fn_ip[4];
static char fn_version[16];

#ifdef _CMOC_VERSION_

static word get16(const byte *p)
{
    return ((word)p[0] << 8) | p[1];
}

static byte os9_link(const char *name, byte **hdr)
{
    byte *h = 0;
    byte err = 0;
    asm {
        ldx     :name
        clra                    ; any type
        pshs    u,y             ; F$Link returns the entry point in Y
        os9     $00             ; F$Link
        tfr     u,x
        puls    u,y
        bcs     @err
        stx     :h
        bra     @done
@err
        stb     :err
@done
    }
    *hdr = h;
    return err;
}

static void os9_unlink(byte *hdr)
{
    asm {
        pshs    u
        ldu     :hdr
        os9     $02             ; F$UnLink
        puls    u
    }
}

static word os9_gblkmp(byte *buf, word *blocks)
{
    word size = 0, n = 0;
    asm {
        ldx     :buf
        pshs    y
        os9     $19             ; F$GBlkMp
        bcs     @err
        std     :size
        sty     :n
@err
        puls    y
    }
    *blocks = n;
    return size;
}

// Copy n bytes at addr in the address space a DAT image describes.
static void os9_cpymem(byte *dat, word addr, word n, byte *dst)
{
    asm {
        pshs    u,y
        ldd     :dat
        ldx     :addr
        ldy     :n
        ldu     :dst
        os9     $1B             ; F$CpyMem
        puls    u,y
    }
}

// Copy the module directory; returns its end in buf, and its address in
// system memory.
static byte *os9_gmoddr(byte *buf, word *sys)
{
    byte *end = 0;
    word s = 0;
    asm {
        ldx     :buf
        pshs    u,y
        os9     $1A             ; F$GModDr
        tfr     y,x
        tfr     u,d
        puls    u,y
        bcs     @err
        stx     :end
        std     :s
@err
    }
    *sys = s;
    return end;
}

static void os9_time(byte *t)
{
    asm {
        ldx     :t
        os9     $15             ; F$Time
    }
}

static void os9_sleep(word ticks)
{
    asm {
        ldx     :ticks
        os9     $0A             ; F$Sleep
    }
}

static void os9_write(const char *p, word n)
{
    asm {
        lda     #1
        ldx     :p
        pshs    y
        ldy     :n
        os9     $8A             ; I$Write
        puls    y
    }
}

// A module name (high bit set on its last letter) is s, ignoring case.
static byte is_name(const byte *m, const char *s)
{
    for (;; m++, s++) {
        if (((*m & 0x7F) | 0x20) != (*s | 0x20))
            return 0;
        if (*m & 0x80)
            return s[1] == 0;
    }
}

static void get_init(void)
{
    byte *init;
    const byte *s;
    byte n = 0;
    if (os9_link("Init", &init))
        return;
    for (s = init + get16(init + 29); *s && n < sizeof os_name - 1; s++)
        os_name[n++] = (char)(*s & 0x7F);
    os_name[n] = 0;
    os9_unlink(init);
}

// The firmware's report on its info port: "label TAB value LF" lines.
// Only the 6309's speed is wanted: "6309 TAB 1.79 MHz (measured), ...".
static void get_mhz(void)
{
    static char report[1100];
    byte *port = (byte *)0xFF43, *data = (byte *)0xFF44;
    word n = 0;
    byte tries, k;
    char c, *p;
    *port = 0xA5;
    for (tries = 0; tries < 30 && *port != 1; tries++)
        os9_sleep(2);
    if (*port != 1)
        return;
    while ((c = (char)*data) != 0)
        if (n < sizeof report - 1)
            report[n++] = c;
    report[n] = 0;
    for (p = report; *p; ) {
        if (!memcmp(p, "6309\t", 5)) {
            p += 5;
            for (k = 0; k < sizeof mhz - 1 && p[k] && p[k] != ' '; k++)
                mhz[k] = p[k];
            mhz[k] = 0;
            if (!strcmp(mhz, "0.00"))
                mhz[0] = 0;
            return;
        }
        while (*p && *p++ != '\n')
            ;
    }
}

static void get_memory(void)
{
    static byte map[1024];
    word bsize, n, i, ram = 0, freeb = 0;
    bsize = os9_gblkmp(map, &n);
    for (i = 0; i < n && i < sizeof map; i++) {
        if (map[i] & 0x80)
            continue;           // not RAM
        ram++;
        if (!map[i])
            freeb++;
    }
    bsize /= 1024;
    ram_k = ram * bsize;
    free_k = freeb * bsize;
}

// The module directory, copied with F$GModDr.  Each entry's module is
// read through its own DAT image, which the copy includes.
static byte moddir[2048];
static byte *modend;
static word modsys;

static void mod_read(const byte *e, word off, word n, byte *dst)
{
    os9_cpymem(moddir + (get16(e) - modsys), get16(e + 4) + off, n, dst);
}

// The first bytes of entry e's module, if it is a device descriptor:
// M$Name at 4, M$PDev (its driver) at 11, M$Port at 14, IT.DTP at $12
// and IT.DRV at $13.
static byte read_desc(const byte *e, byte *hdr)
{
    if (!get16(e))
        return 0;
    mod_read(e, 0, 0x14, hdr);
    return (hdr[6] & 0xF0) == 0xF0;
}

// A module name (high bit set on its last letter) as a C string.
static void copy_name(char *dst, const byte *m, byte size)
{
    byte k = 0;
    while (k < size - 1) {
        dst[k++] = (char)(*m & 0x7F);
        if (*m++ & 0x80)
            break;
    }
    dst[k] = 0;
}

// /DD's descriptor, and another of the same type, driver, port and drive:
// the name its drive also goes by.
static void get_boot(void)
{
    static byte dd[0x14], hdr[0x14], name[32];
    char drv[sizeof dd_driver];
    byte *e, *dde = 0;
    modend = os9_gmoddr(moddir, &modsys);
    if (!modend)
        return;
    for (e = moddir; e < modend && !dde; e += 8) {
        if (!read_desc(e, dd))
            continue;
        mod_read(e, get16(dd + 4), sizeof name, name);
        if (is_name(name, "DD"))
            dde = e;
    }
    if (!dde)
        return;
    dd_found = 1;
    mod_read(dde, get16(dd + 11), sizeof name, name);
    copy_name(dd_driver, name, sizeof dd_driver);
    from_fujinet = is_name(name, "rbdw");
    dd_drive = dd[0x13];
    for (e = moddir; e < modend; e += 8) {
        if (e == dde || !read_desc(e, hdr) || hdr[0x12] != dd[0x12]
            || hdr[0x13] != dd[0x13] || memcmp(hdr + 14, dd + 14, 3))
            continue;
        mod_read(e, get16(hdr + 11), sizeof name, name);
        copy_name(drv, name, sizeof drv);
        if (strcmp(drv, dd_driver))
            continue;
        mod_read(e, get16(hdr + 4), sizeof name, name);
        copy_name(dd_twin, name, sizeof dd_twin);
        return;
    }
}

// Ask the becker port for the time, DriveWire's OP_TIME, and wait up to
// twelve ticks for the six bytes of the answer: year - 1900, month, day,
// hour, minute, second.  Noise on the wire is not a time.
static byte fujinet_answers(void)
{
    byte *status = (byte *)0xFF45, *data = (byte *)0xFF46;
    byte b[6];
    byte got = 0, t;
    while (*status & 2)
        b[0] = *data;           // whatever was left
    *data = 0x23;               // OP_TIME
    for (t = 0; t < 12 && got < 6; t++) {
        while (got < 6 && (*status & 2))
            b[got++] = *data;
        if (got < 6)
            os9_sleep(1);
    }
    return got == 6 && b[1] >= 1 && b[1] <= 12 && b[2] >= 1 && b[2] <= 31
        && b[3] < 24 && b[4] < 60 && b[5] < 61;
}

static void get_fujinet(void)
{
    AdapterConfig ac;
    byte k;
    if (!from_fujinet && !fujinet_answers())
        return;
    if (!fuji_get_adapter_config(&ac))
        return;
    fn_there = 1;
    memcpy(fn_ip, ac.localIP, 4);
    for (k = 0; k < sizeof fn_version - 1 && k < sizeof ac.fn_version && ac.fn_version[k]; k++)
        fn_version[k] = ac.fn_version[k];
    fn_version[k] = 0;
}

#else // made up, to look at on this computer

static void os9_write(const char *p, word n)
{
    fwrite(p, 1, n, stdout);
    fflush(stdout);
}

static void get_init(void)    { strcpy(os_name, "NitrOS-9/6809 Level 2 V03.3.0"); }
static void get_mhz(void)     { strcpy(mhz, "3.24"); }
static void get_memory(void)  { ram_k = 248; free_k = 160; }
static void get_boot(void)
{
    dd_found = from_fujinet = 1;
    strcpy(dd_twin, "X0");
    strcpy(dd_driver, "rbdw");
}
static void get_fujinet(void)
{
    static const byte ip[4] = {192, 168, 1, 232};
    fn_there = 1;
    memcpy(fn_ip, ip, 4);
    strcpy(fn_version, "v1.6.2");
}
static void os9_time(byte *t)
{
    static const byte now[6] = {126, 10, 30, 21, 14, 0};
    memcpy(t, now, 6);
}

#endif

// ── The screen ─────────────────────────────────────────────────────

// The n-th of the NUL-separated names in list.
static const char *nth(const char *list, byte n)
{
    while (n--)
        while (*list++)
            ;
    return list;
}

static const char days[] =
    "Sunday\0Monday\0Tuesday\0Wednesday\0Thursday\0Friday\0Saturday";
static const char months[] =
    "January\0February\0March\0April\0May\0June\0July\0August\0September\0"
    "October\0November\0December";

static byte weekday(word y, byte m, byte d)   // 0 is Sunday
{
    static const byte t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3)
        y--;
    return (byte)((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
}

static void system_lines(void)
{
    byte row = SYS_ROW;
    if (os_name[0]) {
        at(row, SYS_COL);
        sgr("1");
        put(os_name);
        if (strlen(os_name) + 16 <= SYS_WIDTH)
            put(" on the TFR/911H");
        sgr("0");
    }
    row += 2;
    label(row++, SYS_COL, "CPU     ");
    put("Hitachi 6309");
    if (mhz[0]) {
        put(" at ");
        put(mhz);
        put(" MHz");
    }
    label(row++, SYS_COL, "Memory  ");
    putu(ram_k);
    put("K, ");
    putu(free_k);
    put("K free");
    label(row++, SYS_COL, "Booted  ");
    if (dd_found) {                     // "/DD = /H0 (EmuDsk, drive 0)"
        put("/DD");
        if (dd_twin[0]) {
            put(" = /");
            put(dd_twin);
        }
        put(" (");
        put(dd_driver);
        put(", drive ");
        putu(dd_drive);
        putch(')');
    }
    if (when[1] >= 1 && when[1] <= 12) {
        label(row, SYS_COL, "Time    ");
        put(nth(days, weekday(1900 + when[0], when[1], when[2])));
        putch(' ');
        putu(when[2]);
        putch(' ');
        put(nth(months, (byte)(when[1] - 1)));
        putch(' ');
        putu(1900 + when[0]);
        put(", ");
        put2(when[3]);
        putch(':');
        put2(when[4]);
    }
}

static void fujinet_lines(void)
{
    byte k;
    label(FN_ROW, FN_COL, "FujiNet  ");
    if (!fn_there) {
        put("no answer");
        return;
    }
    if (fn_ip[0] | fn_ip[1] | fn_ip[2] | fn_ip[3]) {
        for (k = 0; k < 4; k++) {
            if (k)
                putch('.');
            putu(fn_ip[k]);
        }
    } else
        put("no WiFi");
    label(FN_ROW + 1, FN_COL, "Firmware ");
    put(fn_version);
}

int main(void)
{
    os9_write(ART, ART_LEN);
    get_init();
    get_mhz();
    get_memory();
    get_boot();
    os9_time(when);
    get_fujinet();
    system_lines();
    fujinet_lines();
    at(HINT_ROW, 2);
    sgr(DIM);
    put("man unix: the Unix-style commands.  man tsh: this shell.");
    sgr("0");
    at(END_ROW, 0);
    os9_write(out, outn);
    return 0;
}
