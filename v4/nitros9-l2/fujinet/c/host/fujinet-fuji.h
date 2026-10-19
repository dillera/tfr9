// fujinet-fuji.h -- the FujiNet device calls of fujinet-lib
// (FujiNetWIFI/fujinet-lib) that the FujiNet C programs use, for testing
// them on Unix: see fujinet-host.c.
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#ifndef FUJINET_FUJI_H
#define FUJINET_FUJI_H

#include "fntypes.h"

#define MAX_APPKEY_LEN 64

enum AppKeySize
{
    DEFAULT     // 64 bytes
};

typedef struct
{
    char ssid[33];
    char hostname[64];
    uint8_t localIP[4];
    uint8_t gateway[4];
    uint8_t netmask[4];
    uint8_t dnsIP[4];
    uint8_t macAddress[6];
    uint8_t bssid[6];
    char fn_version[15];
    char sLocalIP[16];
    char sGateway[16];
    char sNetmask[16];
    char sDnsIP[16];
    char sMacAddress[18];
    char sBssid[18];
} AdapterConfigExtended;

// App keys: up to 64 bytes a program keeps on the FujiNet's SD card, named
// by a creator (a person), an application, and a key number.
void fuji_set_appkey_details(uint16_t creator_id, uint8_t app_id, enum AppKeySize keysize);
bool fuji_read_appkey(uint8_t key_id, uint16_t *count, uint8_t *data);
bool fuji_write_appkey(uint8_t key_id, uint16_t count, uint8_t *data);

#endif // FUJINET_FUJI_H
