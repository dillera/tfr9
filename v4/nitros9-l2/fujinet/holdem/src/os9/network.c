/*
  Network functionality: one HTTP GET through the FujiNet's N: device, its
  reply read into buffer.
  @license gpl v. 3, see fujinet/LICENSE for details.
*/

#include "../misc.h"
#include "../platform-specific/network.h"
#include <fujinet-network.h>

uint8_t getResponse(char *url, unsigned char *buffer, uint16_t max_length)
{
    int16_t count;
    if (network_open(url, OPEN_MODE_HTTP_GET, OPEN_TRANS_NONE))
        return 0;
    count = network_read(url, buffer, max_length);
    network_close(url);
    return count > 0;
}
