#!/bin/sh
# run-l2.sh -- boot NitrOS-9 Level 2 on the TFR/911H and talk to it.
#
#   sh run-l2.sh                 # uses $WIRE and $DISK, see below
#   WIRE=/dev/cu.usbmodem31201 sh run-l2.sh
#
# WIRE  USB serial device of the TFR/911H.  Default: /dev/ttyACM0 on Linux;
#       on macOS, the only /dev/cu.usbmodem* device, if there is just one.
# DISK  Disk image served as drive 0 (/DD, /H0).  Default:
#       nitros9-l2/level2.dsk, copied from nitros9-l2/build/ the first time.
#
# ^C goes to OS-9 (aborts the running command).  Quit tether with ^] .
# If the 6309 is already running NitrOS-9, this just reconnects to it.
# Tether's diagnostics go to the file _log.

HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE" || exit 1

DISK="${DISK:-$HERE/nitros9-l2/level2.dsk}"
if [ ! -f "$DISK" ]; then
  BUILT="$HERE/nitros9-l2/build/tfr911-level2.dsk"
  if [ ! -f "$BUILT" ]; then
    echo "No disk image.  Build one with:  make -C $HERE/nitros9-l2" >&2
    exit 1
  fi
  cp "$BUILT" "$DISK" || exit 1
  echo "Created $DISK from $BUILT" >&2
fi

if [ -z "$WIRE" ]; then
  case "$(uname -s)" in
    Darwin)
      set -- /dev/cu.usbmodem*
      if [ $# -ne 1 ] || [ ! -e "$1" ]; then
        echo "Set WIRE to the TFR/911H's serial device; candidates:" >&2
        ls /dev/cu.usbmodem* >&2 2>/dev/null
        exit 1
      fi
      WIRE="$1"
      ;;
    *)
      WIRE=/dev/ttyACM0
      ;;
  esac
fi

go build -o build/tether ./tether/ || exit 1

echo "TFR/911H on $WIRE, disk $DISK.  ^C goes to OS-9; quit with ^] ." >&2
mkdir -p /tmp/pc
exec ./build/tether -wire "$WIRE" -disks "$DISK" -boot l2 -pass_ctrl_c \
  -level 2 -pc /tmp/pc -bind "" "$@" 2>_log
