/*
  App keys: the FujiNet keeps them on its SD card (fujinet-lib's calls).
  @license gpl v. 3, see fujinet/LICENSE for details.
*/

#include "../misc.h"
#include "../platform-specific/appkey.h"
#include <fujinet-fuji.h>

void read_appkey(unsigned int creator_id, unsigned char app_id, unsigned char key_id, char *data)
{
    uint16_t read = 0;
    fuji_set_appkey_details(creator_id, app_id, DEFAULT);
    if (!fuji_read_appkey(key_id, &read, (uint8_t *)data))
        read = 0;
    if (read > MAX_APPKEY_LEN)
        read = MAX_APPKEY_LEN;
    data[read] = 0;
}

void write_appkey(unsigned int creator_id, unsigned char app_id, unsigned char key_id, const char *data)
{
    fuji_set_appkey_details(creator_id, app_id, DEFAULT);
    fuji_write_appkey(key_id, (uint16_t)strlen(data), (uint8_t *)data);
}
