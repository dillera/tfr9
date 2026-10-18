#ifndef BWC_APPKEY_H
#define  BWC_APPKEY_H

#include <stdbool.h>
#include <stdint.h>

// Rich StephensMark Fisher (aka FENROCK [FE0C]) creator id
#define CREATOR_ID 0x0901
#define STOCKS_APP_ID 0x01
#define STOCKS_APP_KEY_1 0x01
#define STOCKS_APP_KEY_2 0x02

bool read_appkey(char *buffer, uint8_t max_len, uint8_t key);
bool write_appkey(char *buffer, uint8_t key);

#endif // BWC_APPKEY_H
