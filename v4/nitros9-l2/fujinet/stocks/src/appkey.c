#include <conio.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "appkey.h"
#include "fujinet-fuji.h"

char read_buffer[65];

/**
 * @brief Read up to max_len bytes from the given app key into buffer.
 *
 * @param buffer  Destination buffer for the key data.
 * @param max_len Maximum number of bytes to read into buffer.
 * @param key     App key identifier to read from.
 * @return true if the key was read successfully and contained data.
 */
bool read_appkey(char *buffer, uint8_t max_len, uint8_t key) {
    bool key_result = false;
    bool read_result = false;
    static uint16_t read_count = 0;

    fuji_set_appkey_details(CREATOR_ID, STOCKS_APP_ID, DEFAULT);

    memset(read_buffer, 0, max_len);
    key_result = fuji_read_appkey(key, &read_count, (uint8_t *)read_buffer);
    if (key_result && read_count > 0)
    {
        strncpy(buffer, read_buffer, max_len);
        if (read_count >= max_len) read_count = max_len - 1;
        read_buffer[read_count] = 0;
        read_result = true;
    }
    return read_result;
}

/**
 * @brief Write the contents of buffer to the given app key.
 *
 * @param buffer Source buffer containing data to write.
 * @param key    App key identifier to write to.
 * @return true if the write succeeded.
 */
bool write_appkey(char *buffer, uint8_t key) {
    bool key_result = false;
    uint16_t write_count = 0;

    fuji_set_appkey_details(CREATOR_ID, STOCKS_APP_ID, DEFAULT);

    key_result = fuji_write_appkey(key, strlen(buffer), (uint8_t *)buffer);

    return key_result;
}
