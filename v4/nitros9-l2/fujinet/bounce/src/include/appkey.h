#ifndef BWC_APPKEY_H
#define  BWC_APPKEY_H

#ifndef _CMOC_VERSION_
#include <stdbool.h>
#include <stdint.h>
#endif

// Mark Fisher (aka FENROCK [FE0C]) creator id
#define CREATOR_ID 0xFE0C
#define BOUNCY_WORLD_APP_ID 0x02
#define APP_KEY_ENDPOINT 0x01
#define APP_KEY_NAME 0x02

#define ENDPOINT_LEN 60
#define NAME_LEN 8

// fujinet-lib 5 has an init_appkey() of its own, in fuji_appkey.c
#define init_appkey bwc_init_appkey
void init_appkey(void);
bool read_appkey(char *buffer, uint8_t max_len, uint8_t key);
bool write_appkey(char *buffer, uint8_t key);

#endif // BWC_APPKEY_H