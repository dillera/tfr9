# FujiNet for NitrOS-9 Level 2 on the TFR/911H

A FujiNet plugged into the TFR/911H gives NitrOS-9 four DriveWire disks
(`/X0`-`/X3`, the FujiNet's device slots 1-4), the network devices `/N`,
`/N1`-`/N5`, and the `fn*` commands that manage the FujiNet: its hosts,
its device slots and its WiFi, a CONFIG program, and programs for the
weather, the news and stock quotes.  NitrOS-9 can also boot from the
FujiNet, with `/DD` on `/X0`.

The commands, the libraries and the driver changes here come from the
FujiNet work in [dillera/nitros9](https://github.com/dillera/nitros9)
(for the Wildbits Jr2; see its `fujinet-uart.md`), moved here so this
repository builds the whole TFR/911H system against a stock NitrOS-9
tree.

## The hardware

The FujiNet is an ESP32 (an ESP32-WROOM DevKitC with 8MB of flash will do)
on a plain 3.3V serial link, wired as the Centipede's FujiNet hat is:

| RP2350B (TFR/911H) | ESP32 |
|---|---|
| GPIO 12, UART0 TX | GPIO 33, UART1 RX |
| GPIO 13, UART0 RX | GPIO 21, UART1 TX |
| GND | GND |

The link runs DriveWire at 921600 baud, 8N1, with no flow control.

The firmware (`../../tfr911h/v4_fujinet.h`) turns that link into a becker
port, the way CoCo emulators present DriveWire:

| Address | Read | Write |
|---|---|---|
| `$FF45` | bit 1 set when a byte from the FujiNet is waiting | |
| `$FF46` | the next byte from the FujiNet | a byte to the FujiNet |

Emulators put the becker port at `$FF41`, but there the TFR/911H has its
sound chip, so NitrOS-9's DriveWire modules are built with
`BECKBASE=$FF45`.  A UART interrupt on the RP2350B's core 0 drains the
receive FIFO into a 4K ring, and the bus loop on core 1 only touches the
rings, so it never stalls a bus cycle.

## The FujiNet's firmware

Build [fujinet-firmware](https://github.com/FujiNetWIFI/fujinet-firmware)
for the board `fujinet-coco-devkitc-centipede` (from PR 1631: BUILD_COCO,
the DriveWire bus, and `COCO_HS_UART`, which fixes the link at 921600 baud
on UART1):

```
cd fujinet-firmware
cat > platformio.local-tfr.ini <<EOF
[fujinet]
build_board = fujinet-coco-devkitc-centipede

[env]
upload_port = /dev/cu.usbserial-XXXX
monitor_port = /dev/cu.usbserial-XXXX
monitor_speed = 460800
EOF
./build.sh -y -l platformio.local-tfr.ini -b -u   # build and flash the firmware
./build.sh -y -l platformio.local-tfr.ini -f      # and its web UI, the first time
```

The firmware must read the client's driver version byte that follows
`OP_DWINIT`; otherwise it takes that byte for the next opcode and throws
away what was already waiting behind it, which at 921600 baud is the next
command, and the 6309 waits for ever for its reply.  fujinet-firmware does
since PR 2027 (merged October 2026).

On a DevKitC the firmware also needs PR 2034 (merged October 2026): the pin
map gave a Safe Reset button to GPIO 14, which a DevKitC leaves
unconnected, with no pull-up.  The floating pin picks up the UART beside
it and reads as presses, each of which restarts the FujiNet; under the
disk traffic of booting from it (`l2fn`), that is every few seconds, and a
request lost in a restart hangs the 6309.  The FujiNet's log shows
`BUTTON_C: SHORT PRESS` and `ACTION: Reboot`.

At `TCL>`, `fujinet` checks the link (it asks the FujiNet the time) and
`fujinet stats` prints the byte counters.  While NitrOS-9 runs, the
firmware's ten-second statistics in the tether's `_log` include the
FujiNet's byte counts and the last 32 bytes on the wire.

## Booting from the FujiNet

The firmware holds two kernels: `l2` boots from the tether's disk (EmuDsk),
and `l2fn` from the FujiNet's device slot 1 (DriveWire drive 0).  At power
up the FujiNet puts its own CONFIG disk in drive 0, so `l2fn` first mounts
whatever is in slot 1 (`fujinet mount 1` does just that; `l2fn -n` skips
it).

1. Make a disk that boots from the FujiNet: a copy of your `level2.dsk`,
   your files and all, with the FujiNet's `OS9Boot`:

   ```
   make fujinet-disk                # level2.dsk -> level2-fn.dsk
   ```

2. Put it where the FujiNet can mount it.  Its SD card is easiest: the
   FujiNet's web server has WebDAV at `/dav`:

   ```
   curl -X MKCOL http://FUJINET-IP/dav/tfr911
   curl -T level2-fn.dsk http://FUJINET-IP/dav/tfr911/level2-fn.dsk
   ```

   where FUJINET-IP is the address `fnstatus` shows (or copy it to the
   card, or serve it from a TNFS server).

3. Put it in device slot 1, read/write.  From NitrOS-9 booted the usual way
   (`l2`), with host slot 1 being the FujiNet's SD card:

   ```
   OS9: fnsetdevfile 0 1 2 /tfr911/level2-fn.dsk
   ```

   or with `fnconfig`, or the FujiNet's web page.  The FujiNet remembers
   it.

4. Boot it:  `BOOT=l2fn sh ../run-l2.sh`, or type `l2fn` at `TCL>`.

```
FujiNet slot 1: level2-fn.dsk
...
NitrOS-9 Level 2 on the TFR/911H
Shell
OS9: devs
DD   070000  rbdw     9700  RBF      05
Term 07FF14  sc6850   9500  SCF      02
```

When booted from the FujiNet, the tether's disk is still there as `/H0`.
Don't change `/X0` (device slot 1) from `fnconfig` while it is `/DD`, and
remember that changing the WiFi drops the connection `/X0` may depend on.

The FujiNet keeps the image file open.  After you replace the file on the
SD card or the TNFS server, `l2fn` (which remounts it) or `fujinet mount 1`
makes the FujiNet open the new one.

## Commands

| Command | What it does |
|---|---|
| `fnstatus` | The FujiNet's firmware version, host name, WiFi network and IP settings |
| `fnlisthosts` | The eight host slots |
| `fngethost <host_slot>` | One host slot |
| `fnsethost <host_slot> <host>` | Set one host slot, keeping the other seven |
| `fnmount <host_slot>` | Mount a host |
| `fnlistdevs` | The four DriveWire device slots (`/X0`-`/X3`) |
| `fngetdevfile <device_slot>` | One device slot's image |
| `fnsetdevfile <device_slot> <host_slot> <mode> <path>` | Put an image in a device slot; mode 1 is read-only, 2 read/write |
| `fnmountimg <device_slot> [<mode>]` | Mount a device slot's image |
| `fnconfig` | FujiNet CONFIG: status, hosts, device slots (with a directory browser), WiFi |
| `fnweather` | The weather where you are, now and for the week, with pictures (FujiNetWIFI/fujinet-weather) |
| `fnwx` | The first `fnweather`, in assembly: the weather now, in words |
| `fnnews` | The news, from the FujiNet news server (FujiNetWIFI/fujinet-news) |
| `fnstocks` | Ten stocks and a ticker, from Finnhub (FujiNetWIFI/fujinet-stocks) |
| `fnbounce` | Bounce World: shapes bouncing across many computers' screens (markjfisher/bounce-world-client) |
| `fnbattleship` | Fuji Battleship: sea battle for up to four players, in color and with sound (FujiNetWIFI/fujinet-battleship) |

Slots are numbered from 0 here (device slot 0 is `/X0`), as the FujiNet
numbers them on the wire.

`fnconfig` and `fnwx` were written for a CoCo 3 window.  Built here
with `ANSI=1` (`lib/fnansi.as`), they draw with ANSI escape sequences for
the tether's terminal, which should be at least 80 by 24.

## Programs in C

`fnnews`, `fnstocks`, `fnbounce`, `fnweather` and `fnbattleship` are the
CoCo versions of five FujiNet programs, written in C against fujinet-lib, the FujiNet's C
library, and compiled with CMOC, which can compile for OS-9.

* `fujinet-lib/` is fujinet-lib for NitrOS-9: FujiNetWIFI/fujinet-lib
  v5.0.1 (its `include/` and `common/`, unchanged) with a bus for
  NitrOS-9, `bus/os9`, which makes each exchange with the FujiNet one
  `SS.Fuji` call on `/N1`.  Every `fuji_*` and `network_*` call is there,
  so other CoCo fujinet-lib programs can be built the same way.  Its
  README tells how it works; `make fujinet-lib` builds
  `build/fnlib/libfujinet.a`.
* `c/term.c` is the tether's terminal: raw keys (Esc and ^C are the
  CoCo's BREAK, and the arrows arrive as they are, with bit 7 of `$FF42`
  set), ANSI colors, and a copy of the screen, so that a row can be shown
  in reverse video and back again, which the CoCo programs do by flipping
  the attribute bytes in screen memory.  `term_frame()` draws a frame in
  the copy alone and then sends only what changed, which is what the CoCo
  does with two screens; the line-drawing and block characters
  (`TG_HLINE` and the rest) go out as UTF-8.  Output is written straight
  to the console's 6850 (`$FF15`), as SCF takes some 300 us a character.

`news/` is the CoCo client of
[fujinet-news](https://github.com/FujiNetWIFI/fujinet-news)
(`clients/src/coco`, commit b351d24) with the CoCo 3's 80-column layout,
in color; its screen code became `news/screen.c`.  `I` shows where the
news comes from.  `stocks/` is
[fujinet-stocks](https://github.com/FujiNetWIFI/fujinet-stocks) (commit
1ececd8) with an `os9` platform (`include/os9/`, `src/os9/`) beside its
`coco` one; its own files changed in a few lines (`BUILD_OS9`: an
80-column screen, Esc for BREAK, colors, and the ticker drawn only when it
moves).  The stock list is in the same app keys as on other computers.
`bounce/` is
[bounce-world-client](https://github.com/markjfisher/bounce-world-client)
(commit 5ac9408), the client of Mark Fisher's
[Bounce World](https://github.com/markjfisher/bounce-world), with an `os9`
platform (`src/os9/`) beside its `coco` one.  The world is 40 or 80
places wide, set on the title screen (`W`); at 40 each place is two
columns of the terminal, so that round shapes look round.  The title
screen, with the version and the date of the build, and the screen of
shapes are NitrOS-9's own (`src/os9/title.c`), for an 80-column terminal.
Its own files changed in a few lines (`BUILD_OS9`: the places, a color
for each kind of shape, calls to those screens, and a main loop that
sends the next request before it draws), and two for fujinet-lib 5, which
has an `init_appkey()` of its own and makes `fn_network_bw` and its kin
macros.
Give it the server's framed TCP port, 9003 (`bouncy.diller.org:9003`);
9002 is for clients older than 2.3.
`weather/` is fnweather, after
[fujinet-weather](https://github.com/FujiNetWIFI/fujinet-weather)'s CoCo
client (commit f6e8fd4): its requests to ip-api.com and Open-Meteo, its
weather codes and icons, with a screen of its own for the terminal.  It
asks in three requests, not seven (the fewest whose URLs fit the 255
characters of a device spec), in metric, converting in integers, so `U`
needs no request.  The icons are half-block pixel art in the terminal's
colors, made from shapes by `weather/icons.py` into `icondata.h`.  The
last weather, the place and the units are kept in `/DD/SYS/fnweather.dat`
and shown at once; it fetches again when they are half an hour old.
`battleship/` is fnbattleship,
[fujinet-battleship](https://github.com/FujiNetWIFI/fujinet-battleship)
(commit 87b8cc7) with a directory for NitrOS-9, `src/os9`, beside
`src/coco`: the screens for an 80 by 24 terminal (a square is two
columns; two players side by side with coordinates, three or four in
quarters), sounds on the SN76489, and a title with a battleship made by
`src/os9/shipart.py`.  Of the game's own files, a few lines changed
(`BUILD_OS9`): a coordinate can be typed (a letter and a digit), the ship
being placed is marked in the fleet, the logo is NitrOS-9's, and how to
play lists the keys.  One fix is for every computer: when a turn's time
runs out, the client asks the server at once (upstream waited out the
time again, over and over, while the server had moved for it).  It brings its own
graphics characters to `c/term.c` (`term_glyphs()`).

For trying a change without the board, `make build/fnnews-host`,
`make build/fnstocks-host`, `make build/fnweather-host`,
`make build/fnbattleship-host` and `make build/fnbounce-host` build them for
this computer, with `c/host/fujinet-host.c` (curl, python3, and a socket
for TCP) as the FujiNet; `fnbounce` is compiled as for CMOC, with
`bounce/host/`'s stand-ins for CMOC's headers.  App keys are files in
`$FN_APPKEYS`.

## How it works

* `dwio` (`modules/dwio.asm`), built with `BECKER=1`, `BECKBASE=$FF45` and
  `DWIO_NO_VIRQ=1`, is DriveWire's wire routines.  It has no virtual-serial
  poller, which would send `OP_SERREAD` in the background and steal the
  FujiNet's raw replies.
* `rbdw` and the descriptors `x0`-`x3` are NitrOS-9's own DriveWire disks;
  `ddx0` is `/X0` named `/DD`.  `boot_dw` loads `OS9Boot` for `l2fn`.
* `scdwv` (`modules/scdwv.asm`) adds the `SS.Fuji` SetStat: an atomic
  FujiNet exchange, the request written and the reply read with interrupts
  masked.  In Level 2 it copies both through a system buffer.
* `lib/fuji.as` is the FujiNet's `OP_FUJI` (`$E2`) management protocol:
  a command, then `GET_ERROR` for its status and `GET_RESPONSE` for its
  data, all through `/N1` and `SS.Fuji`.  `lib/fnnet.as` is the
  `OP_FUJINET` (`$E3`) network protocol, for HTTP and JSON.
* `defs/drivewire.d` is NitrOS-9's, with the FujiNet's opcodes and
  `SS.Fuji`.  It is found ahead of the NitrOS-9 tree's, and
  `lib/sys6809l2.as` makes the linked commands' system definitions from it.

`../rmake-l2.tcl` lists the modules (`FUJINET_MODS`), and `../Makefile`
builds the commands and the programs in C.  `tests/` has the source checks from dillera/nitros9:
`python3 -m unittest discover -s tests`.

## License

What is here for the FujiNet is under the GNU General Public License,
version 3, as the FujiNet project's programs are: see `LICENSE`.  That is
fujinet-lib (which has its own copy of the license), the terminal and the
host stand-in in `c/`, the commands in `cmds/` and their libraries
`lib/fnnet.as` and `lib/fnansi.as`, the tests, fnweather, and what was
written for NitrOS-9 in the other programs (their `os9` directories and
`host` stand-ins).  The files written here say so at the top.

Some files came from elsewhere and keep their own terms:

* NitrOS-9's: `modules/dwio.asm`, `modules/scdwv.asm`, `lib/fuji.as`,
  `lib/sys6809l2.as`, `defs/drivewire.d`, `cmds/fngetdevfile.as` and
  `cmds/fnsetdevfile.as`.
* The programs' own files, from fujinet-news and fujinet-battleship, whose
  sources say GPL v3, and from fujinet-stocks and
  markjfisher/bounce-world-client, which state no license; those remain
  their authors'.

