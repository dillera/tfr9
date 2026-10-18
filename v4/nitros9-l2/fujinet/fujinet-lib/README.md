# fujinet-lib for NitrOS-9

This is [fujinet-lib](https://github.com/FujiNetWIFI/fujinet-lib), the
FujiNet's C library, for C programs on NitrOS-9 compiled with CMOC: every
`fuji_*` and `network_*` call of fujinet-lib v5.0.1, so a program written
for the FujiNet on a CoCo builds for NitrOS-9 on the TFR/911H.  `fnnews`
and `fnstocks` (`../news`, `../stocks`) use it.

## What is here

| Directory | |
|---|---|
| `include/`, `common/` | fujinet-lib's, unchanged: FujiNetWIFI/fujinet-lib `main`, commit 7bf70b8 (2026-10-18), version v5.0.1 |
| `bus/os9/` | the bus for NitrOS-9, made from fujinet-lib's `bus/coco` |
| `cmoc-cpp` | the C preprocessor for CMOC (see Building) |
| `LICENSE` | fujinet-lib's licence, the GNU GPL v3, which covers these files |

fujinet-lib keeps what is the same on every computer in `common/`, and what
each computer's bus needs in `bus/<platform>/`.  On a CoCo the bus calls
the DriveWire ROM: `dwwrite()` sends bytes, `dwread()` reads the reply, and
everything else in `bus/coco` (`fuji_bus_call`, `bus_ready`, the status and
response calls) is built on those two.

## The NitrOS-9 bus

Under NitrOS-9 the DriveWire wire belongs to the system, and a program
reaches the FujiNet through the `SS.Fuji` SetStat of scdwv on `/N1`
(`../modules/scdwv.asm`), which sends a request and reads its reply with
interrupts masked, so that nothing else on the wire takes the reply.  So
in `bus/os9`:

* `ssfuji.c` opens `/N1` when first needed and makes the `SS.Fuji` calls.
* `dwwrite()` queues its bytes; `dwread()` sends what is queued and reads
  the reply, as one `SS.Fuji` call.  Every fujinet-lib command ends with a
  read (at least of its status), so nothing is left queued.  A request
  longer than `SSFUJI_MAX` (512 bytes, scdwv's `FUJI$MaxTran`) goes ahead
  in pieces.
* `network_bus.c` asks for at most 512 bytes of a reply at a time, which
  `SS.Fuji` can carry; `network_read()` asks again for the rest.
* `fuji_bus_call.s` is `bus/coco`'s, made position independent, as an OS-9
  module must be: `LBSR` for calls, its variables through `Y` (CMOC's data
  pointer) and its constant table through `PCR`.
* `network_proceed.c`: the becker port has no proceed line, so
  `network_has_proceed()` is false; poll `network_status()`.
* `strncat.c`: CMOC 0.1.97's library has no `strncat()`.

The other files of `bus/os9` are `bus/coco`'s, unchanged.  The wire
protocol is the CoCo's (`FUJI_PLATFORM_COCO`), since NitrOS-9 speaks to the
same FujiNet firmware.

The becker port has no timeout: a request the FujiNet takes seconds over,
such as the first status of an HTTP GET or a JSON parse, holds the 6309,
with interrupts masked, until it is done.

## Building

`make fujinet-lib` in `v4/nitros9-l2` builds `build/fnlib/libfujinet.a`; the
programs link with it (`-L build/fnlib -lfujinet`, so that only what they
use is linked).  By hand:

```
cmoc --os9 --cpp=$PWD/cmoc-cpp -Iinclude -Ibus/os9 -c -o x.o common/network_open.c
lwar -a -r libfujinet.a *.o
cmoc --os9 --cpp=$PWD/cmoc-cpp -Iinclude -c myprog.c
cmoc --os9 -o myprog myprog.o -L. -lfujinet
```

fujinet-lib indents some preprocessor directives, which macOS's `cpp` (a
traditional preprocessor) skips; `cmoc-cpp` runs the C compiler's own
preprocessor instead.  Give `--add-os9-stack-space` when compiling the file
with `main()`: CMOC ignores it when linking.

## Updating

Copy `include/` and `common/` from a newer fujinet-lib, and compare its
`bus/coco` with `bus/os9`: the files above are the ones that differ.
