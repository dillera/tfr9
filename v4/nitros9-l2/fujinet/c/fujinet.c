// fujinet.c -- fujinet-lib's calls for C programs on NitrOS-9.
//
// Each exchange with the FujiNet is one SS.Fuji SetStat on /N1 (see
// ../modules/scdwv.asm): the request written and the raw reply read with
// interrupts masked, so nothing else on the DriveWire wire can take the
// reply.  The parameter block is the reply's length, then the request:
//
//   network device  E3 unit cmd aux1 aux2 [data]
//   FujiNet device  E2 cmd [data]
//
// A command's reply is fetched afterwards: $02 asks for its status (1 is
// success), $01 for its data.  Every reply that is buffered must be
// fetched, as the firmware appends the next one to it.  The becker port
// has no timeout: a request the FujiNet takes seconds over (the first
// STATUS of an HTTP GET, a JSON parse) holds the 6309 until it is done.
//
// The protocol is lib/bus/drivewire in FujiNetWIFI/fujinet-firmware;
// ../lib/fnnet.as and ../lib/fuji.as are the same in assembly language.

#include "fujinet-fuji.h"
#include "fujinet-network.h"

#define OP_FUJI     0xE2
#define OP_FUJINET  0xE3

#define CMD_RESPONSE 0x01
#define CMD_ERROR    0x02

#define FN_CHUNK 480        // bytes per READ: a reply is at most 512

static byte fn_path = 255;  // /N1
static byte blk[2 + 5 + 256];   // the reply's length, then the request

static void fn_sleep(word ticks) {
    asm {
        ldx     :ticks
        os9     $0A             ; F$Sleep
    }
}

// One exchange: the request (len bytes at blk+2), then rlen bytes of reply.
static byte xfer(word len, void *reply, word rlen) {
    static char name[] = "/N1\r";
    char *nm = name;
    byte *b = blk;
    byte path = 255, err = 0;
    word n = len + 2;
    if (fn_path == 255) {
        asm {
            lda     #3          ; UPDAT.
            ldx     :nm
            os9     $84         ; I$Open
            bcs     @bad
            sta     :path
            bra     @done
@bad
            stb     :err
@done
        }
        if (err) return err;
        fn_path = path;
    }
    path = fn_path;
    blk[0] = (byte)(rlen >> 8);
    blk[1] = (byte)rlen;
    asm {
        lda     :path
        ldx     :b
        pshs    u,y
        ldy     :n
        ldu     :reply          ; last: U is the frame pointer
        ldb     #$E2            ; SS.Fuji
        os9     $8E             ; I$SetStt
        puls    u,y
        bcc     @ok
        stb     :err
@ok
    }
    return err;
}

// ── The network device ─────────────────────────────────────────────

// Any data goes at blk+7 first.
static byte net(byte unit, byte cmd, byte aux1, byte aux2, word datalen,
                void *reply, word rlen) {
    blk[2] = OP_FUJINET;
    blk[3] = unit;
    blk[4] = cmd;
    blk[5] = aux1;
    blk[6] = aux2;
    return xfer(5 + datalen, reply, rlen);
}

static byte net_ok(byte unit) {
    byte e = 0;
    return !net(unit, CMD_ERROR, 0, 0, 0, &e, 1) && e == 1;
}

static byte net_reply(byte unit, void *buf, word len) {
    return net(unit, CMD_RESPONSE, (byte)(len >> 8), (byte)len, 0, buf, len);
}

// A devicespec or a query: 256 bytes, NUL padded.
static void net_spec(const char *s) {
    memset(blk + 7, 0, 256);
    strncpy((char *)blk + 7, s, 255);
}

static byte unit_of(const char *devicespec) {
    if (devicespec[1] == ':') return 1;
    if (devicespec[2] == ':') return (byte)(devicespec[1] - '0');
    return 1;
}

uint8_t network_open(const char *devicespec, uint8_t mode, uint8_t trans) {
    byte unit = unit_of(devicespec);
    net_spec(devicespec);
    if (net(unit, 'O', mode, trans, 256, 0, 0) || !net_ok(unit))
        return FN_ERR_IO_ERROR;
    return FN_ERR_OK;
}

uint8_t network_close(const char *devicespec) {
    byte unit = unit_of(devicespec);
    if (net(unit, 'C', 0, 0, 0, 0, 0) || !net_ok(unit)) return FN_ERR_IO_ERROR;
    return FN_ERR_OK;
}

uint8_t network_status(const char *devicespec, uint16_t *bw, uint8_t *c, uint8_t *err) {
    byte unit = unit_of(devicespec);
    byte st[4];
    if (net(unit, 'S', 0, 0, 0, 0, 0) || net_reply(unit, st, 4)) return FN_ERR_IO_ERROR;
    *bw = ((word)st[0] << 8) | st[1];   // big-endian, as for a CoCo
    *c = st[2];
    *err = st[3];
    return FN_ERR_OK;
}

int16_t network_read(const char *devicespec, void *buf, uint16_t len) {
    byte unit = unit_of(devicespec);
    byte *p = (byte *)buf;
    word total = 0, n, avail, idle = 0;
    byte conn, err;
    while (len) {
        if (network_status(devicespec, &avail, &conn, &err))
            return total ? (int16_t)total : -FN_ERR_IO_ERROR;
        if (!avail) {
            // Nothing yet: at the end, on an error, or after ten seconds, stop.
            if (err != 1 || !conn || ++idle > 600) break;
            fn_sleep(1);
            continue;
        }
        idle = 0;
        n = len < avail ? len : avail;
        if (n > FN_CHUNK) n = FN_CHUNK;
        if (n == 256) n = 255;  // fnnet.as found exactly 256 troublesome in Level 2
        if (net(unit, 'R', (byte)(n >> 8), (byte)n, 0, 0, 0) || net_reply(unit, p + total, n))
            return total ? (int16_t)total : -FN_ERR_IO_ERROR;
        total += n;
        len -= n;
    }
    return (int16_t)total;
}

uint8_t network_json_parse(const char *devicespec) {
    byte unit = unit_of(devicespec);
    if (net(unit, 0xFC, 1, 0, 0, 0, 0) || !net_ok(unit)) return FN_ERR_IO_ERROR;   // JSON
    if (net(unit, 'P', 0, 0, 0, 0, 0) || !net_ok(unit)) return FN_ERR_IO_ERROR;
    return FN_ERR_OK;
}

// The value is read as one READ of NETWORK_JSON_QUERY_MAX bytes, which the
// firmware pads with NULs, and ends at the first NUL, CR or LF.
int16_t network_json_query(const char *devicespec, const char *query, char *s) {
    byte unit = unit_of(devicespec);
    word i;
    s[0] = 0;
    net_spec(query);
    if (net(unit, 'Q', 0, 0, 256, 0, 0)) return -FN_ERR_IO_ERROR;
    if (net(unit, 'R', 0, NETWORK_JSON_QUERY_MAX, 0, 0, 0)
        || net_reply(unit, s, NETWORK_JSON_QUERY_MAX))
        return -FN_ERR_IO_ERROR;
    for (i = 0; i < NETWORK_JSON_QUERY_MAX - 1; i++)
        if (!s[i] || s[i] == '\r' || s[i] == '\n') break;
    s[i] = 0;
    return (int16_t)i;
}

// ── The FujiNet device: app keys ───────────────────────────────────

static uint16_t ak_creator;
static uint8_t ak_app;

// Any data goes at blk+4 first.
static byte fuji(byte cmd, word datalen, void *reply, word rlen) {
    blk[2] = OP_FUJI;
    blk[3] = cmd;
    return xfer(2 + datalen, reply, rlen);
}

static byte fuji_ok(void) {
    byte e = 0;
    return !fuji(CMD_ERROR, 0, &e, 1) && e == 1;
}

void fuji_set_appkey_details(uint16_t creator_id, uint8_t app_id, enum AppKeySize keysize) {
    (void)keysize;
    ak_creator = creator_id;
    ak_app = app_id;
}

// OPEN_APPKEY: creator (big-endian, as for a CoCo), app, key, mode, 0.
static byte appkey_open(byte key, byte mode) {
    blk[4] = (byte)(ak_creator >> 8);
    blk[5] = (byte)ak_creator;
    blk[6] = ak_app;
    blk[7] = key;
    blk[8] = mode;
    blk[9] = 0;
    return !fuji(0xDC, 6, 0, 0) && fuji_ok();
}

// READ_APPKEY's reply is the length (little-endian), then 64 bytes.
bool fuji_read_appkey(uint8_t key_id, uint16_t *count, uint8_t *data) {
    byte r[2 + MAX_APPKEY_LEN];
    word n;
    *count = 0;
    if (!appkey_open(key_id, 0)) return false;
    if (fuji(0xDD, 0, 0, 0) || !fuji_ok()) return false;
    if (fuji(CMD_RESPONSE, 0, r, sizeof r)) return false;
    n = r[0] | ((word)r[1] << 8);
    if (n > MAX_APPKEY_LEN) n = MAX_APPKEY_LEN;
    memcpy(data, r + 2, n);
    *count = n;
    return true;
}

// WRITE_APPKEY: the length (big-endian), then always 64 bytes.
bool fuji_write_appkey(uint8_t key_id, uint16_t count, uint8_t *data) {
    if (count > MAX_APPKEY_LEN) count = MAX_APPKEY_LEN;
    if (!appkey_open(key_id, 1)) return false;
    blk[4] = (byte)(count >> 8);
    blk[5] = (byte)count;
    memset(blk + 6, 0, MAX_APPKEY_LEN);
    memcpy(blk + 6, data, count);
    return !fuji(0xDE, 2 + MAX_APPKEY_LEN, 0, 0) && fuji_ok();
}
