# Build NitrOS-9 Level 2 for the TFR/911H.
#
#   make            -- build/tfr911-level2.dsk and ../tfr911h/nitros9_l2_rom.h
#
# NITROS9_DIR selects the NitrOS-9 source tree (default: ../../../nitros9).
# LWASM, LWLINK and OS9_TOOL may be overridden if not on $PATH.

B = build/tfr911/level2
OS9_TOOL ?= os9
NITROS9_DIR ?= ../../../nitros9
export NITROS9_DIR

# Newer NitrOS-9 trees ship Basic09 only as a prebuilt disk, not source.
B09DSK = $(NITROS9_DIR)/3rdparty/packages/basic09/Basic09_v010100.dsk

all: build/tfr911-level2.dsk ../tfr911h/nitros9_l2_rom.h

$B/Makefile: rmake-l2.tcl
	tclsh rmake-l2.tcl

$B/tfr911-level2.dsk: $B/Makefile FORCE
	rm -f $B/tfr911-level2.dsk
	$(MAKE) -C $B

build/tfr911-level2.dsk: $B/tfr911-level2.dsk startup
	cp -f $< $@
	$(OS9_TOOL) copy -l -r startup $@,startup
	$(OS9_TOOL) attr -q -r -w -pr $@,startup
	if ! $(OS9_TOOL) dir $@,CMDS | grep -qw basic09 && test -f $(B09DSK); then \
	  for f in basic09_6809:basic09 runb_6809:runb inkey:inkey syscall:syscall; do \
	    $(OS9_TOOL) copy -r $(B09DSK),CMDS/$${f%%:*} $@,CMDS/$${f##*:} && \
	    $(OS9_TOOL) attr -q -r -w -e -pr -pe $@,CMDS/$${f##*:} || exit 1; \
	  done; \
	fi

build/launcher.raw: launcher.asm
	mkdir -p build
	$(or $(LWASM),lwasm) --format=raw -o'$@' '$<'

../tfr911h/nitros9_l2_rom.h: build/launcher.raw $B/tfr911-level2.dsk
	python3 gen-rom-header.py build/launcher.raw $B/tfr911-level2.t35 > $@

clean:
	rm -rf build

FORCE:
.PHONY: all clean FORCE
