# NitrOS-9 Level 2 on the TFR/911H

The v4 TFR/911H firmware can boot NitrOS-9 Level 2 as well as TurbOS.
At the `TCL>` prompt, `bye` starts TurbOS and `l2` starts NitrOS-9 Level 2.

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

## Games

`games/` holds C programs compiled with [CMOC](http://perso.b2b2c.ca/~sarrazip/dev/cmoc.html)
(`cmoc --os9`); if `cmoc` isn't installed they are left off the disk.

* `tetris`: arrows or a/d/w/s, space drops, p pauses, q quits.  It draws
  with ANSI escapes on the terminal the tether runs in, and opens its own
  path to `/term` for raw keys, so the Shell's terminal settings are untouched.

Arrow keys reach OS-9 as a CoCo 3 sends them: left `$08`, right `$09`,
up `$0C`, down `$0A` (so left arrow is also backspace at the Shell).

Two CMOC-on-OS-9 lessons: give programs `--add-os9-stack-space=2048` (the
default stack is too small even for printf), and don't write
`flag ? byteval : -1` (CMOC gives it the byte's type, so -1 becomes 255).

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

`modules/` holds TFR/911H-specific modules, which take precedence over
NitrOS-9's.  The modules in `OS9Boot` and the commands on the disk are
listed at the end of `rmake-l2.tcl`.

### Prerequisites

* A NitrOS-9 source tree, by default `../../../nitros9` (next to `tfr9` on
  the shelf).  Set `NITROS9_DIR` to use another.
* `lwasm` from lwtools, `tclsh`, `python3`.
* `os9` from toolshed.  Set `OS9_TOOL=/path/to/os9` if it isn't on `$PATH`.

```
NITROS9_DIR=$HOME/coco-shelf/nitros9 make OS9_TOOL=$HOME/toolshed/build/unix/os9/os9
```
