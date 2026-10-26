# NitrOS-9 Level 2 on the TFR/911H

The v4 TFR/911H firmware can boot NitrOS-9 Level 2 as well as TurbOS.
At the `TCL>` prompt, `bye` starts TurbOS and `l2` starts NitrOS-9 Level 2.

The *TFR Manual and Reference*, `doc/tfr-manual.pdf`, describes everything
here in the manner of a 1990s ULTRIX manual; `make manual` rebuilds it.
Its companion, the *TFR Installation Guide*, `doc/tfr-install-guide.pdf`,
takes a release (the first is `dist/jamesBrown1` at the top of this
repository) from flashing the FujiNet to booting NitrOS-9 from it;
`make install-guide` rebuilds it.

## Quick start

```
cd tfr9/v4
make nitros9-l2      # build NitrOS-9 and its disk image (see Prerequisites)
make build/tfr911h_v4.uf2 && make flash      # or picotool load -x build/tfr911h_v4.uf2
WIRE=/dev/cu.usbmodemXXXX sh run-l2.sh        # macOS; on Linux WIRE defaults to /dev/ttyACM0
```

`run-l2.sh` starts the tether with the options Level 2 needs:

* `-disks nitros9-l2/level2.dsk` serves the disk image as EmuDsk drive 0
  (`/DD` and `/H0`).  The first run copies it from `nitros9-l2/build/`;
  after that it is yours, and `make nitros9-l2` will not overwrite it.
  Set `DISK=...` to use another image.
* `-boot l2` types `l2` at the `TCL>` prompt, so it boots without help.
  If NitrOS-9 is already running, the tether just reconnects to it.
* `-pass_ctrl_c` sends ^C to OS-9 (to abort a command), so quit the
  tether with ^] instead.

```
NitrOS-9/6809 Level 2 V3.3.0
Tandy Color Computer 3
(C) 2014 The NitrOS-9 Project
...
NitrOS-9 Level 2 on the TFR/911H

Shell

OS9:mfree

 Blk Begin   End   Blks  Size
 --- ------ ------ ---- ------
   8  10000  3DFFF   17   184k
                   ==== ======
            Total:   17   184k
```

## Unix-style commands

`unix.tcl` adds `cd` and `exit` to the Shell (by patching a copy of
`shell_21.asm`), `pwd`/`pxd` (NitrOS-9's `pd`), `grep` (patched to take an
unquoted pattern), `clear`, and aliases that run the OS-9 equivalent:
`ls` (`ls -l`), `ll`, `cat`, `cp`, `mv`, `rm`, `mkdir`, `rmdir`, `ps`, `df`,
`less`, `man`, `uname`.  `man unix` on the TFR/911H summarizes them.
An alias is a tiny module that F$Chains to the real command; add more to
`UNIX_ALIASES` in `unix.tcl`.

To bring an existing disk up to date with a new build (new OS9Boot, and
everything in CMDS and SYS; your other files are left alone), quit the
tether, then:

```
make install DISK=level2.dsk OS9_TOOL=...   # then press RESET on the board
```

## The shell: tsh

`startup` ends with `tsh </term`, so after booting you type to `tsh`
(`tsh/tsh.c`, in C).  It reads each line itself and then hands it to the
OS-9 Shell, so commands keep the Shell's syntax (`!`, `<`, `>`, `>>`, `#`,
`&`, `;`, procedure files).  It adds:

* line editing: Left/Right, `^A` `^E`, Esc-b/Esc-f, Backspace, `^D`, `^W`,
  `^K` `^U` `^X`, `^Y`, `^T`, `^L`, `^C`; long lines scroll sideways;
* history: Up/Down, Shift-Up/Down for lines that start as this one does,
  `^R` to search; the last 100 lines are kept in `/DD/SYS/tsh_history`
  (written ten at a time, and on exit);
* Tab completion of command names (built-in commands, aliases, the
  execution directory, the path) and file names;
* a search path for commands not in memory or the execution directory
  (`path`, `PATH=`), set in `/DD/SYS/tshrc`; a command found there is
  loaded by `tsh` and unlinked when it ends;
* `cd -`, `alias`, `history`, `which`, `source`, `set`, `PROMPT=`
  (`%d` shows the directory).

`exit` leaves `tsh`, and after booting that gives the old Shell.  `tsh`
reads keys on its own path to `/term` with echo and the signal keys off,
and sets bit 7 of `$FF42` while editing for the arrow keys (as `vi` does).
`make build/tsh-host` builds it for this computer, running commands with
`/bin/sh`, for testing.  `man tsh` on the board, and tsh(1) in the manual,
describe it all.  `make install` leaves a disk's own `startup` alone; add
`tsh </term` to it to start `tsh` when booting.

## Games

`games/` holds C programs compiled with [CMOC](http://perso.b2b2c.ca/~sarrazip/dev/cmoc.html)
(`cmoc --os9`); if `cmoc` isn't installed they are left off the disk.

* `tetris`: arrows or a/d/w/s, space drops, p pauses, q quits.  It draws
  with ANSI escapes on the terminal the tether runs in, and opens its own
  path to `/term` for raw keys, so the Shell's terminal settings are untouched.

Arrow keys reach OS-9 as a CoCo 3 sends them: left `$08`, right `$09`,
up `$0C`, down `$0A` (so left arrow is also backspace at the Shell),
except while a program has set bit 7 of `$FF42` (see Sound).

* `info`: everything the system knows about itself: the OS version and
  settings (from the Init module), memory (the block map), processes,
  device descriptors and which are attached, each disk's size and free
  space (from LSN 0 and its bitmap), the IRQ polling table, the modules in
  memory, and the TFR/911H firmware's own report (Pico clock and ID, the
  6309's measured speed and load, interrupt rate, disk, console and sound
  counters, uptime).  It reads the kernel's tables with `F$GPrDsc`,
  `F$CpyMem` and `F$GModDr`, as `procs`, `devs` and `mdir` do.  `info -p`
  leaves out the bold headings, for redirecting to a file.

* `vi` (or `vim`): a small vi, with most of the normal-mode commands
  people use without thinking (motions, `d`/`c`/`y` with any motion,
  counts, `p`, `u` with any number of levels, `.`, `/` and `?`),
  insert mode with working arrow keys, and `:w :q :wq :x :e :r :N` and
  `:[range]s/old/new/[g]`.  `man vi` lists it all.  Searches are plain
  text, not patterns.  Files are written with OS-9's CR line ends; files
  with LF or CR LF ends are read too.  It holds about 25K of text.

  `make build/vi-host` builds it for this computer.  Typed the same keys,
  it leaves the same file as Vim does for 120-odd editing commands.

* `zm`: a Z-machine, for Infocom's version 3 games (Zork I-III,
  Hitchhiker's Guide, Planetfall, Enchanter, Deadline, ...) and Inform
  games compiled with `-v3`.  `zm zork1` looks for `zork1` or `zork1.z3`
  here and then in `/DD/GAMES`; `man zm` lists its options.  It has a status
  line, word wrap and `[MORE]` for an 80x24 terminal (`-w`, `-h` for others),
  and `save` and `restore` write files in the current data directory.
  Dynamic memory stays in RAM, and as much of the rest of the story as fits
  (about 40K); the rest is read from disk in 512-byte pages as needed.

  To put story files on a disk, quit the tether, then
  `make stories STORIES="zork1.z3 ..." DISK=level2.dsk`.  Zork I-III are
  MIT-licensed now, compiled in `historicalsource/zork1` (and `zork2`,
  `zork3`) on GitHub, under `COMPILED/`.

  `make build/zm-host` builds `zm` for this computer, for testing:
  `build/zm-host -d story.z3 < commands` plays without escape sequences,
  and its output can be compared with `dfrotz`'s.  It passes all of CZECH's
  v3 tests.

`tetris` has sound if a speaker is connected (see below): the Tetris theme
(`m` turns it off and on), and effects for moves, landings, line clears and
game over.

Two CMOC-on-OS-9 lessons: give programs `--add-os9-stack-space=2048` (the
default stack is too small even for printf), and don't write
`flag ? byteval : -1` (CMOC gives it the byte's type, so -1 becomes 255).

## Sound

`../tfr911h/v4_audio.h` makes sound as PWM on GPIO 8, which is
pin 1 of the 2x10 header J3 and is not connected to the CPU.  An Adafruit
PAM8302 amplifier drives a small speaker from it:

| J3 pin | Signal | PAM8302 |
|---|---|---|
| 2 | VBUS (5 V) | Vin |
| 6 | GND | GND and A- |
| 1 | GPIO 8 | A+, through 1 kOhm, with 10 nF from A+ to GND |

The speaker goes on the PAM8302's + and - terminals.  Start with its volume
pot turned down.  At the `TCL>` prompt, `beep 440 1000` plays an A for one
second (`beep ?hz? ?ms?`), and `click off` / `click on` turn off and on
the keyclicks.

In NitrOS-9:

* Every key typed clicks.  `click off` and `click on` turn this off and on.
* The terminal bell (`$07`, as `beep` sends) beeps.
* An SN76489 sound chip (three tones and a noise channel, as in the Game
  Master Cartridge) is at `$FF41`; `tetris` plays through it.
* `$FF42` is a control register: bit 0 enables keyclicks, bit 1 the bell.
  Bit 7 (set by `vi`) sends arrow and page keys to OS-9 as the tether's
  codes 128-133, instead of the CoCo's codes, where left and right are
  the same as Backspace and Tab.

## FujiNet

With a FujiNet on GPIO 12/13, NitrOS-9 has four DriveWire disks
(`/X0`-`/X3`, the FujiNet's device slots 1-4), the network devices `/N`,
`/N1`-`/N5`, and the commands that manage the FujiNet: `fnstatus`,
`fnlisthosts`, `fngethost`, `fnsethost`, `fnmount`, `fnlistdevs`,
`fngetdevfile`, `fnsetdevfile`, `fnmountimg`, `fnconfig` (FujiNet CONFIG),
and the programs `fnweather`, `fnnews`, `fnstocks`, `fnbounce` and
`fnbattleship` (and `fnwx`, the first `fnweather`).  It can boot from
the FujiNet too: `l2fn` at `TCL>`, or

```
make fujinet-disk                 # level2.dsk -> level2-fn.dsk, booting from the FujiNet
BOOT=l2fn sh ../run-l2.sh         # once level2-fn.dsk is in the FujiNet's device slot 1
```

`fujinet/README.md` tells how: the wiring, the FujiNet's firmware, putting
the disk on the FujiNet, and how it all works.

## What the firmware emulates

`../tfr911h/v4_nitros9_l2.h` makes the TFR/911H look like a small CoCo 3,
the same way v3 ran Level 2 on the TFR/905H:

| Device | Address | Notes |
|---|---|---|
| RAM | | 256K (`L2_RAM_KILOS`), so 184K is free for programs |
| GIME MMU | `$FF90`, `$FF91`, `$FFA0`-`$FFAF` | two tasks, constant page at `$FE00` |
| GIME VBORD IRQ | `$FF92` | 60 Hz, from a Pico timer |
| MC6850 ACIA | `$FF14`-`$FF15` | the console, `/Term` |
| EmuDsk | `$FF80`-`$FF86` | virtual hard disk, sectors moved over USB by the tether |
| RTC | `$FFC0`-`$FFC7` | the PC's local time, sent by the tether (`clock2_tfr911`) |
| SN76489 | `$FF41` | write-only sound chip (see Sound) |
| Sound control | `$FF42` | bit 0 keyclicks, bit 1 bell, bit 7 raw arrow keys |
| Info port | `$FF43`-`$FF44` | write `$A5` to `$FF43`; when it reads 1, read the firmware's report from `$FF44` (`label TAB value LF` lines, then 0) |
| Becker port | `$FF45`-`$FF46` | DriveWire to a FujiNet: `$FF45` bit 1 says a byte is waiting at `$FF46` (see `fujinet/`) |

Other `$FFxx` locations read back what was last written.
The 6309 runs in 6809 mode.

## Building

`make` here runs `rmake-l2.tcl` (adapted from `v3.decprecated/rmake.tcl`) to
generate `build/tfr911/level2/Makefile`, which assembles the modules straight
from a NitrOS-9 source tree.  It produces:

* `build/tfr911/level2/tfr911-level2.t35`: rel, boot_emu and krn, which the
  firmware puts at `$2600` behind a small launcher at `$2500`
  (`../tfr911h/nitros9_l2_rom.h`, which is checked in so the firmware builds
  without NitrOS-9).
* `build/tfr911-level2.dsk`: a 99,999-sector disk with `OS9Boot`, `CMDS` and
  `startup`.  The file is sparse at first and grows as sectors are written.
* `build/tfr911/level2/tfr911-level2-fn.t35` and `-fn.o9b`: the same, but
  booting from DriveWire drive 0 (`boot_dw`, and `/DD` on `/X0`), for
  `l2fn` (`../tfr911h/nitros9_l2fn_rom.h`), and `build/tfr911-level2-fn.dsk`.

`modules/` holds TFR/911H-specific modules, which take precedence over
NitrOS-9's, as do the FujiNet's modules and definitions in `fujinet/`.
The FujiNet commands are built from `fujinet/cmds/`, and `fnnews`,
`fnstocks`, `fnbounce`, `fnweather` and `fnbattleship` from the C in
`fujinet/news/`, `fujinet/stocks/`, `fujinet/bounce/`, `fujinet/weather/`
and `fujinet/battleship/`, with the
terminal in `fujinet/c/` and fujinet-lib for NitrOS-9 in
`fujinet/fujinet-lib/` (`make fujinet-lib`).  The modules in `OS9Boot` and the commands on the disk are
listed at the end of `rmake-l2.tcl`.

### Prerequisites

* A NitrOS-9 source tree, by default `../../../nitros9` (next to `tfr9` on
  the shelf).  Set `NITROS9_DIR` to use another.
* `lwasm` from lwtools, `tclsh`, `python3`.
* `os9` from toolshed.  Set `OS9_TOOL=/path/to/os9` if it isn't on `$PATH`.

```
NITROS9_DIR=$HOME/coco-shelf/nitros9 make OS9_TOOL=$HOME/toolshed/build/unix/os9/os9
```
