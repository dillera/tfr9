// fujinet-network.h -- the network calls of fujinet-lib
// (FujiNetWIFI/fujinet-lib) that the FujiNet C programs use, for testing
// them on Unix: see fujinet-host.c.

#ifndef FUJINET_NETWORK_H
#define FUJINET_NETWORK_H

#include "fntypes.h"

#define FN_ERR_OK         0x00
#define FN_ERR_IO_ERROR   0x01
#define FN_ERR_BAD_CMD    0x02
#define FN_ERR_OFFLINE    0x03
#define FN_ERR_WARNING    0x04
#define FN_ERR_NO_DEVICE  0x05
#define FN_ERR_UNKNOWN    0xFF

#define OPEN_MODE_READ        0x04
#define OPEN_MODE_WRITE       0x08
#define OPEN_MODE_RW          0x0C
#define OPEN_MODE_HTTP_GET    0x04
#define OPEN_MODE_HTTP_PUT    0x08
#define OPEN_MODE_HTTP_GET_H  0x0C

#define OPEN_TRANS_NONE  0x00
#define OPEN_TRANS_CR    0x01
#define OPEN_TRANS_LF    0x02
#define OPEN_TRANS_CRLF  0x03

// network_json_query() reads a value of up to this many bytes, less one
// for the NUL: its buffer must be at least this big.
#define NETWORK_JSON_QUERY_MAX 64

// A devicespec is "N:url" (unit 1) or "Nn:url" (unit n, 1..8).
uint8_t network_open(const char *devicespec, uint8_t mode, uint8_t trans);
uint8_t network_close(const char *devicespec);
// bw: bytes waiting, c: connected, err: 1 if all is well, 136 at the end.
uint8_t network_status(const char *devicespec, uint16_t *bw, uint8_t *c, uint8_t *err);
// Reads len bytes, or what there is to the end; returns the count, or a
// negative FN_ERR_* if nothing could be read.
int16_t network_read(const char *devicespec, void *buf, uint16_t len);
// Reads what is there, up to len bytes, waiting for something.
int16_t network_read_nb(const char *devicespec, void *buf, uint16_t len);
uint8_t network_write(const char *devicespec, const void *buf, uint16_t len);
uint8_t network_json_parse(const char *devicespec);
int16_t network_json_query(const char *devicespec, const char *query, char *s);

#endif // FUJINET_NETWORK_H
