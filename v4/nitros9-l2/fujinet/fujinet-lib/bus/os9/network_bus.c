#include "fb_header.h"
#include "fujinet-fuji-coco.h"
#include "fujinet-network-coco.h"
#include "dw.h"
#include "ssfuji.h"

#include <fujinet-int.h>
#include <fujinet-bus.h>
#include <fujinet-commands.h>

uint16_t network_bus_read(uint8_t device, void *buffer, size_t length)
{
  fujibus_header fb_header;


  // NitrOS-9: one SS.Fuji call brings at most SSFUJI_MAX bytes back, and
  // network_read() asks again for the rest.  A reply of exactly 256 bytes
  // is avoided, as tfr9's fujinet/lib/fnnet.as found it troublesome in Level 2.
  if (length > SSFUJI_MAX)
    length = SSFUJI_MAX;
  if (length == 256)
    length = 255;

  fb_header.opcode = OP_NET;
  fb_header.fn.net.unit = device - FUJI_DEVICEID_NETWORK + 1;
  fb_header.fn.net.cmd = NETCMD_READ;

  bus_ready();
  dwwrite((uint8_t *) &fb_header, sizeof(fb_header));
  dwwrite((uint8_t *) &length, sizeof(length));
  // A failed READ has no response, but SEND_RESPONSE would pad one with zeros.
  if (network_get_error(fb_header.fn.net.unit))
    return 0;
  if (network_get_response(fb_header.fn.net.unit, (uint8_t *) buffer, length))
    return 0;

  return length;
}

uint16_t network_bus_write(uint8_t device, const void *buffer, size_t length)
{
  fujibus_header fb_header;


  fb_header.opcode = OP_NET;
  fb_header.fn.net.unit = device - FUJI_DEVICEID_NETWORK + 1;
  fb_header.fn.net.cmd = NETCMD_WRITE;

  bus_ready();
  dwwrite((uint8_t *) &fb_header, sizeof(fb_header));
  dwwrite((uint8_t *) &length, sizeof(length));
  dwwrite((uint8_t *) buffer, length);

  if (network_get_error(fb_header.fn.net.unit))
    length = 0;
  return length;
}
