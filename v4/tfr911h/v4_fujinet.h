#ifndef V4_FUJINET_H_
#define V4_FUJINET_H_

// v4_fujinet.h — a FujiNet on the TFR/911H, as a DriveWire becker port.
//
// The FujiNet is an ESP32 running fujinet-firmware built for
// fujinet-coco-devkitc-centipede (BUILD_COCO, DRIVEWIRE bus, COCO_HS_UART):
// DriveWire over a plain 3.3V UART at 921600 baud, 8N1, no flow control.
// It is wired the way the Centipede's FujiNet hat is:
//
//   RP2350B GPIO 12 (UART0 TX)  ->  ESP32 GPIO 33 (UART1 RX)
//   RP2350B GPIO 13 (UART0 RX)  <-  ESP32 GPIO 21 (UART1 TX)
//
// The 6309 sees a becker port, as a CoCo emulator presents one, but at
// $FF45/$FF46 because $FF41/$FF42 are the sound chip and its control:
//
//   $FF45  read: bit 1 set when a byte from the FujiNet is waiting
//   $FF46  read: the next byte from the FujiNet;  write: a byte to it
//
// NitrOS-9's dwio, rbdw and boot_dw are built with BECKER=1 and
// BECKBASE=$FF45 (see ../nitros9-l2/rmake-l2.tcl).
//
// The bus loop (core 1) must answer every cycle, and the UART FIFOs are
// only 32 bytes deep, so core 1 only touches two cross-core rings.  On
// core 0, a UART interrupt fills the receive ring (at 921600 baud the FIFO
// fills in 350us, faster than the background loop always comes round), and
// the background loop empties the transmit ring onto the wire.

#include <hardware/irq.h>
#include <hardware/uart.h>

namespace fujinet {

#define FUJINET_UART uart0
constexpr uint BAUD = 921600;
constexpr uint TX_PIN = 12;
constexpr uint RX_PIN = 13;
constexpr uint STATUS_PORT = 0x45;  // $FF45
constexpr uint DATA_PORT = 0x46;    // $FF46

CrossCoreFIFO<byte, 4096> rx_ring;  // FujiNet -> 6309: interrupt, then core 1
CrossCoreFIFO<byte, 1024> tx_ring;  // 6309 -> FujiNet: core 1, then core 0

volatile uint32_t stat_rx_bytes, stat_tx_bytes;
volatile uint32_t stat_rx_drops;    // rx_ring full: a FujiNet byte lost
volatile uint32_t stat_tx_drops;    // tx_ring full: a 6309 byte lost
volatile uint32_t stat_overruns;    // the UART's own FIFO overran
volatile uint32_t stat_empty_reads; // $FF46 read with nothing waiting
bool inited;

// The last bytes on the wire, for the L2 stats log: bit 8 set for bytes
// from the FujiNet.  Both writers are on core 0 (the interrupt and Pump),
// so at worst an entry is lost when one interrupts the other.
uint16_t trace[32];
volatile uint trace_pos;
FORCE_INLINE void Trace(uint v) { trace[trace_pos++ & 31] = v; }

void IN_RAM UartIrq() {
  uart_hw_t* hw = uart_get_hw(FUJINET_UART);
  if (hw->rsr & UART_UARTRSR_OE_BITS) {
    stat_overruns++;
    hw->rsr = UART_UARTRSR_BITS;  // any write clears the error bits
  }
  while (uart_is_readable(FUJINET_UART)) {
    byte b = (byte)uart_getc(FUJINET_UART);
    if (rx_ring.push(b)) stat_rx_bytes++;
    else stat_rx_drops++;
    Trace(0x100 | b);
  }
}

// Core 0, once at boot.  The interrupt belongs to core 0 because core 1
// runs the bus loop with interrupts off.
void Init() {
  gpio_set_function(TX_PIN, UART_FUNCSEL_NUM(FUJINET_UART, TX_PIN));
  gpio_set_function(RX_PIN, UART_FUNCSEL_NUM(FUJINET_UART, RX_PIN));
  gpio_pull_up(RX_PIN);  // idle high if nothing is connected
  uart_init(FUJINET_UART, BAUD);
  uart_set_format(FUJINET_UART, 8, 1, UART_PARITY_NONE);
  uart_set_hw_flow(FUJINET_UART, false, false);
  uart_set_fifo_enabled(FUJINET_UART, true);
  irq_set_exclusive_handler(UART_IRQ_NUM(FUJINET_UART), UartIrq);
  irq_set_enabled(UART_IRQ_NUM(FUJINET_UART), true);
  uart_set_irqs_enabled(FUJINET_UART, true, false);
  inited = true;
}

// Core 0, from the background loop.
void IN_RAM Pump() {
  byte b;
  while (uart_is_writable(FUJINET_UART) && tx_ring.pop(b)) {
    uart_putc_raw(FUJINET_UART, b);
    stat_tx_bytes++;
    Trace(b);
  }
}

// "FujiNet: 12 out, 6 in, 0 lost; last: >23 <7e <0a ..." for the log.
int Report(char* p, int room) {
  int n = snprintf(p, room, "FujiNet: %lu out, %lu in, %lu lost, %lu empty reads; last:",
                   (unsigned long)stat_tx_bytes, (unsigned long)stat_rx_bytes,
                   (unsigned long)(stat_rx_drops + stat_tx_drops + stat_overruns),
                   (unsigned long)stat_empty_reads);
  uint end = trace_pos, count = end < 32 ? end : 32;
  for (uint k = end - count; k != end && n < room - 5; k++) {
    uint v = trace[k & 31];
    n += snprintf(p + n, room - n, " %c%02x", (v & 0x100) ? '<' : '>', v & 0xFF);
  }
  return n;
}

// Throw away anything waiting in either direction.  Core 0, while the
// 6309 is not running.
void Flush() {
  byte b;
  while (tx_ring.pop(b)) {}
  while (rx_ring.pop(b)) {}
}

// ── The becker port, for the bus loop on core 1 ──

FORCE_INLINE byte Status() {
  return rx_ring.size() ? 0x02 : 0x00;
}

FORCE_INLINE byte Read() {
  byte b;
  if (!rx_ring.pop(b)) {
    stat_empty_reads++;
    b = 0xFF;
  }
  return b;
}

FORCE_INLINE void Write(byte b) {
  if (!tx_ring.push(b)) stat_tx_drops++;
}

// ── For the Tcl REPL (core 0, before the 6309 starts) ──

// Send `out`, then collect up to `want` bytes or until `ms` pass.
uint Transact(const byte* out, uint n, byte* in, uint want, uint ms) {
  Flush();
  for (uint k = 0; k < n; k++) tx_ring.push(out[k]);
  uint got = 0;
  uint64_t deadline = time_us_64() + ms * 1000;
  while ((got < want || tx_ring.size()) && time_us_64() < deadline) {
    Pump();
    byte b;
    if (got < want && rx_ring.pop(b)) in[got++] = b;
  }
  return got;
}

// ── FujiNet commands: OP_FUJI ($E2), a command byte and its parameters ──
// (the protocol fujinet/lib/fuji.as uses; see fujinet-firmware's
// lib/bus/drivewire).  The reply to a command is fetched separately: its
// status with GET_ERROR, its data with GET_RESPONSE.

constexpr byte OP_FUJI = 0xE2;
constexpr byte FUJI_GET_RESPONSE = 0x01, FUJI_GET_ERROR = 0x02;
constexpr byte FUJI_READ_DEVICE_SLOTS = 0xF2, FUJI_MOUNT_HOST = 0xF9,
               FUJI_MOUNT_IMAGE = 0xF8;
constexpr byte FUJI_OK = 0x01;
constexpr uint DEVICE_SLOT_SIZE = 38;  // host slot, mode, file name[36]
constexpr uint DEVICE_SLOTS = 4;       // DriveWire drives 0-3

// Send a command, then ask how it went.  A mount can take a TNFS server
// a few seconds, so wait a while for the status.
bool FujiCmd(const byte* req, uint n) {
  byte status = 0;
  Transact(req, n, nullptr, 0, 50);
  const byte get_error[2] = {OP_FUJI, FUJI_GET_ERROR};
  return Transact(get_error, 2, &status, 1, 15000) == 1 && status == FUJI_OK;
}

// Mount the image in device slot `slot` (0-3) the way FujiNet CONFIG
// does: its host first, then the image, read-only or read/write as the
// slot says.  At power-up the FujiNet puts its own CONFIG disk in
// DriveWire drive 0, so l2fn does this for slot 0 before booting.
// Returns nullptr, or what went wrong.
const char* MountSlot(uint slot, char* name, uint name_size) {
  const byte read_slots[2] = {OP_FUJI, FUJI_READ_DEVICE_SLOTS};
  if (!FujiCmd(read_slots, 2)) return "no answer from the FujiNet";
  byte slots[DEVICE_SLOTS * DEVICE_SLOT_SIZE];
  const byte get_response[2] = {OP_FUJI, FUJI_GET_RESPONSE};
  if (Transact(get_response, 2, slots, sizeof slots, 2000) < sizeof slots)
    return "the FujiNet did not send its device slots";
  const byte* d = slots + slot * DEVICE_SLOT_SIZE;
  snprintf(name, name_size, "%.36s", (const char*)d + 2);
  if (d[0] >= 8 || !name[0]) return "nothing is in that device slot";
  const byte mount_host[3] = {OP_FUJI, FUJI_MOUNT_HOST, d[0]};
  if (!FujiCmd(mount_host, 3)) return "the FujiNet could not mount the slot's host";
  const byte mount_image[4] = {OP_FUJI, FUJI_MOUNT_IMAGE, (byte)slot, d[1] == 2 ? (byte)2 : (byte)1};
  if (!FujiCmd(mount_image, 4)) return "the FujiNet could not mount the image";
  return nullptr;
}

}  // namespace fujinet

// Tcl:  fujinet          ask the FujiNet the time (DriveWire OP_TIME),
//                        to show the link works
//       fujinet mount [N]  mount device slot N (1-4, default 1)
//       fujinet stats    byte and error counters
int fujinet_cmd(ClientData, Tcl_Interp* interp, int argc, char* argv[]) {
  static char result[160];
  if (argc > 1 && !strcmp(argv[1], "mount")) {
    int n = argc > 2 ? atoi(argv[2]) : 1;
    if (n < 1 || n > (int)fujinet::DEVICE_SLOTS) {
      Tcl_SetResult(interp, (char*)"device slots are 1 to 4", TCL_STATIC);
      return TCL_ERROR;
    }
    char name[40];
    const char* err = fujinet::MountSlot(n - 1, name, sizeof name);
    if (err) snprintf(result, sizeof result, "slot %d: %s", n, err);
    else snprintf(result, sizeof result, "slot %d: mounted %s", n, name);
    Tcl_SetResult(interp, result, TCL_STATIC);
    return err ? TCL_ERROR : TCL_OK;
  }
  if (argc > 1 && !strcmp(argv[1], "stats")) {
    snprintf(result, sizeof result,
             "rx %lu tx %lu rx_drops %lu tx_drops %lu overruns %lu empty_reads %lu",
             (unsigned long)fujinet::stat_rx_bytes, (unsigned long)fujinet::stat_tx_bytes,
             (unsigned long)fujinet::stat_rx_drops, (unsigned long)fujinet::stat_tx_drops,
             (unsigned long)fujinet::stat_overruns, (unsigned long)fujinet::stat_empty_reads);
    Tcl_SetResult(interp, result, TCL_STATIC);
    return TCL_OK;
  }
  const byte op_time = 0x23;  // reply: year-1900 month day hour minute second
  byte t[6];
  uint got = fujinet::Transact(&op_time, 1, t, sizeof t, 1000);
  if (got < sizeof t) {
    snprintf(result, sizeof result,
             "no answer from the FujiNet (%u of 6 bytes; GPIO %u TX, %u RX, %u baud)",
             got, fujinet::TX_PIN, fujinet::RX_PIN, fujinet::BAUD);
    Tcl_SetResult(interp, result, TCL_STATIC);
    return TCL_ERROR;
  }
  snprintf(result, sizeof result, "FujiNet says it is %04d-%02d-%02d %02d:%02d:%02d",
           1900 + t[0], t[1], t[2], t[3], t[4], t[5]);
  Tcl_SetResult(interp, result, TCL_STATIC);
  return TCL_OK;
}

#endif  // V4_FUJINET_H_
