// fujinet-host.c -- fujinet.c for testing the FujiNet C programs on Unix:
// HTTP with curl, JSON queries with python3, and app keys in files named
// appkey-CCCC-AA-KK in $FN_APPKEYS (default: the current directory).

#include "fujinet-fuji.h"
#include "fujinet-network.h"
#include <unistd.h>

static char *body;          // what the URL returned
static size_t body_len, body_pos;
static char url[300];
static char json_file[] = "/tmp/fnhost-json.XXXXXX";
static int json_ready;

static void fetch(void) {
    char cmd[400];
    FILE *f;
    size_t n;
    if (body) return;
    snprintf(cmd, sizeof cmd, "curl -s -L --max-time 20 '%s'", url);
    f = popen(cmd, "r");
    body = malloc(1);
    body_len = body_pos = 0;
    if (!f) return;
    for (;;) {
        body = realloc(body, body_len + 4096);
        n = fread(body + body_len, 1, 4096, f);
        if (!n) break;
        body_len += n;
    }
    pclose(f);
}

uint8_t network_open(const char *devicespec, uint8_t mode, uint8_t trans) {
    const char *p = strchr(devicespec, ':');
    (void)mode;
    (void)trans;
    free(body);
    body = 0;
    json_ready = 0;
    snprintf(url, sizeof url, "%s", p ? p + 1 : devicespec);
    return FN_ERR_OK;
}

uint8_t network_close(const char *devicespec) {
    (void)devicespec;
    free(body);
    body = 0;
    return FN_ERR_OK;
}

uint8_t network_status(const char *devicespec, uint16_t *bw, uint8_t *c, uint8_t *err) {
    size_t left;
    (void)devicespec;
    fetch();
    left = body_len - body_pos;
    *bw = left > 65535 ? 65535 : (uint16_t)left;
    *c = left != 0;
    *err = left ? 1 : 136;
    return FN_ERR_OK;
}

int16_t network_read(const char *devicespec, void *buf, uint16_t len) {
    size_t left;
    (void)devicespec;
    fetch();
    left = body_len - body_pos;
    if (len > left) len = (uint16_t)left;
    memcpy(buf, body + body_pos, len);
    body_pos += len;
    return (int16_t)len;
}

uint8_t network_json_parse(const char *devicespec) {
    int fd;
    (void)devicespec;
    fetch();
    strcpy(json_file, "/tmp/fnhost-json.XXXXXX");
    fd = mkstemp(json_file);
    if (fd < 0 || write(fd, body, body_len) != (ssize_t)body_len) return FN_ERR_IO_ERROR;
    close(fd);
    json_ready = 1;
    return FN_ERR_OK;
}

int16_t network_json_query(const char *devicespec, const char *query, char *s) {
    char cmd[600];
    FILE *f;
    size_t n;
    (void)devicespec;
    s[0] = 0;
    if (!json_ready) return -FN_ERR_IO_ERROR;
    snprintf(cmd, sizeof cmd,
             "python3 -c 'import json,sys\n"
             "v=json.load(open(sys.argv[1]))\n"
             "for k in sys.argv[2].strip(\"/\").split(\"/\"):\n"
             "  v=v[int(k)] if isinstance(v,list) else v.get(k,\"\")\n"
             "print(v if not isinstance(v,(dict,list)) else \"\",end=\"\")' %s '%s' 2>/dev/null",
             json_file, query);
    f = popen(cmd, "r");
    if (!f) return -FN_ERR_IO_ERROR;
    n = fread(s, 1, NETWORK_JSON_QUERY_MAX - 1, f);
    pclose(f);
    s[n] = 0;
    return (int16_t)n;
}

static uint16_t ak_creator;
static uint8_t ak_app;

void fuji_set_appkey_details(uint16_t creator_id, uint8_t app_id, enum AppKeySize keysize) {
    (void)keysize;
    ak_creator = creator_id;
    ak_app = app_id;
}

static FILE *appkey_file(uint8_t key_id, const char *mode) {
    char name[300];
    const char *dir = getenv("FN_APPKEYS");
    snprintf(name, sizeof name, "%s/appkey-%04X-%02X-%02X", dir ? dir : ".",
             ak_creator, ak_app, key_id);
    return fopen(name, mode);
}

bool fuji_read_appkey(uint8_t key_id, uint16_t *count, uint8_t *data) {
    FILE *f = appkey_file(key_id, "rb");
    *count = 0;
    if (!f) return true;
    *count = (uint16_t)fread(data, 1, MAX_APPKEY_LEN, f);
    fclose(f);
    return true;
}

bool fuji_write_appkey(uint8_t key_id, uint16_t count, uint8_t *data) {
    FILE *f = appkey_file(key_id, "wb");
    if (!f) return false;
    fwrite(data, 1, count > MAX_APPKEY_LEN ? MAX_APPKEY_LEN : count, f);
    fclose(f);
    return true;
}
