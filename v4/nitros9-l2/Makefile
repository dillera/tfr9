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

LWASM ?= lwasm
LWLINK ?= lwlink
LWAR ?= lwar
# Games are C, compiled with CMOC (http://perso.b2b2c.ca/~sarrazip/dev/cmoc.html).
CMOC ?= cmoc
GAMES = $(if $(shell command -v $(CMOC)),build/tetris build/zm build/vi build/info)
# The FujiNet commands (see fujinet/README.md).
FN_CMDS = $(patsubst fujinet/cmds/%.as,build/fn/%,$(wildcard fujinet/cmds/*.as))
# Z-machine story files to put in /DD/GAMES, e.g. STORIES="~/if/*.z3".
STORIES ?=

OBJ_FLAGS = --6309 --format=obj --no-warn=ifp1 \
  --pragma=pcaspcr,nosymbolcase,condundefzero,undefextern,dollarnotlocal,noforwardrefmax,export

all: build/tfr911-level2.dsk build/tfr911-level2-fn.dsk \
  ../tfr911h/nitros9_l2_rom.h ../tfr911h/nitros9_l2fn_rom.h

$B/Makefile: rmake-l2.tcl unix.tcl
	tclsh rmake-l2.tcl

$B/tfr911-level2.dsk: $B/Makefile FORCE
	rm -f $B/tfr911-level2.dsk
	$(MAKE) -C $B
$B/tfr911-level2-fn.t35 $B/tfr911-level2-fn.o9b: $B/tfr911-level2.dsk

build/tfr911-level2.dsk: $B/tfr911-level2.dsk startup build/grep build/helpmsg $(GAMES) $(FN_CMDS)
	cp -f $< $@
	$(OS9_TOOL) copy -l -r startup $@,startup
	$(OS9_TOOL) attr -q -r -w -pr $@,startup
	$(OS9_TOOL) copy -r build/grep $@,CMDS/grep
	$(OS9_TOOL) attr -q -r -w -e -pr -pe $@,CMDS/grep
	for g in $(GAMES) $(FN_CMDS); do \
	  $(OS9_TOOL) copy -r $$g $@,CMDS/$$(basename $$g) && \
	  $(OS9_TOOL) attr -q -r -w -e -pr -pe $@,CMDS/$$(basename $$g) || exit 1; \
	done
	@test -n "$(GAMES)" || echo "NOTE: $(CMOC) not found, so no games on the disk."
	$(if $(STORIES),$(MAKE) stories DISK=$@)
	$(OS9_TOOL) makdir $@,SYS
	$(OS9_TOOL) copy -l -r $(NITROS9_DIR)/level1/sys/errmsg $@,SYS/errmsg
	$(OS9_TOOL) copy -l -r build/helpmsg $@,SYS/helpmsg
	if ! $(OS9_TOOL) dir $@,CMDS | grep -qw basic09 && test -f $(B09DSK); then \
	  for f in basic09_6809:basic09 runb_6809:runb inkey:inkey syscall:syscall; do \
	    $(OS9_TOOL) copy -r $(B09DSK),CMDS/$${f%%:*} $@,CMDS/$${f##*:} && \
	    $(OS9_TOOL) attr -q -r -w -e -pr -pe $@,CMDS/$${f##*:} || exit 1; \
	  done; \
	fi

# grep and the fn* commands are linked with NitrOS-9's alib, built here
# from its sources.
build/alib.a:
	mkdir -p build/alib
	for f in $(NITROS9_DIR)/lib/alib/*.as; do \
	  $(LWASM) $(OBJ_FLAGS) -I$(NITROS9_DIR)/defs -I$(NITROS9_DIR)/lib/alib \
	    -o build/alib/$$(basename $$f .as).o $$f || exit 1; \
	done
	rm -f $@ && $(LWAR) -c $@ build/alib/*.o

build/sys.o: fujinet/lib/sys6809l2.as fujinet/defs/drivewire.d
	mkdir -p build
	$(LWASM) $(OBJ_FLAGS) -Ifujinet/defs -I$(NITROS9_DIR)/defs -o $@ $<

build/grep: $B/Makefile build/sys.o build/alib.a  # rmake-l2.tcl writes build/local/grep.as
	$(LWASM) $(OBJ_FLAGS) -I$(NITROS9_DIR)/defs -o build/grep.o build/local/grep.as
	cd build && $(LWLINK) --format=os9 -o grep grep.o sys.o alib.a  # module name = output name

# The FujiNet commands, each linked with the FujiNet libraries.
# (No nosymbolcase here: fnweather and fnconfig have labels that differ
# only in case, as in NitrOS-9's own rules for linked commands.)
# ANSI=1: fnconfig and fnweather draw with ANSI sequences, for the
# tether's terminal, instead of CoCo 3 window codes (fujinet/lib/fnansi.as).
FN_FLAGS = $(subst nosymbolcase$(COMMA),,$(OBJ_FLAGS)) -DANSI=1 \
  -Ifujinet/lib -Ifujinet/defs -I$(NITROS9_DIR)/defs
COMMA = ,
build/fn/%.o: fujinet/lib/%.as fujinet/defs/drivewire.d
	mkdir -p build/fn
	$(LWASM) $(FN_FLAGS) -o $@ $<
build/fn/%.o: fujinet/cmds/%.as fujinet/defs/drivewire.d fujinet/lib/fnansi.as
	mkdir -p build/fn
	$(LWASM) $(FN_FLAGS) -o $@ $<
build/fn/%: build/fn/%.o build/fn/fnnet.o build/fn/fuji.o build/sys.o build/alib.a
	cd build/fn && $(LWLINK) --format=os9 -o $* $*.o fnnet.o fuji.o ../sys.o ../alib.a

build/tetris: games/tetris.c
	mkdir -p build
	cd build && $(CMOC) --os9 --add-os9-stack-space=2048 -o tetris ../games/tetris.c  # module name = output name

build/zm: games/zm.c
	mkdir -p build
	cd build && $(CMOC) --os9 --function-stack=0 --add-os9-stack-space=2048 -o zm ../games/zm.c

build/vi: games/vi.c
	mkdir -p build
	cd build && $(CMOC) --os9 --function-stack=0 --add-os9-stack-space=1024 -o vi ../games/vi.c

build/info: games/info.c
	mkdir -p build
	cd build && $(CMOC) --os9 --function-stack=0 --add-os9-stack-space=1024 -o info ../games/info.c

# vi for this computer, for testing
build/vi-host: games/vi.c
	mkdir -p build
	$(CC) -O2 -o $@ games/vi.c

# zm for this computer, for testing:  build/zm-host -d story.z3
build/zm-host: games/zm.c
	mkdir -p build
	$(CC) -O2 -Wall -o $@ games/zm.c

# NitrOS-9's help text, plus the Unix-style commands.
build/helpmsg: helpmsg.unix
	mkdir -p build
	cat $(NITROS9_DIR)/level2/coco3/sys/helpmsg helpmsg.unix > $@

build/launcher.raw: launcher.asm
	mkdir -p build
	$(or $(LWASM),lwasm) --format=raw -o'$@' '$<'

../tfr911h/nitros9_l2_rom.h: build/launcher.raw $B/tfr911-level2.dsk
	python3 gen-rom-header.py build/launcher.raw $B/tfr911-level2.t35 > $@

../tfr911h/nitros9_l2fn_rom.h: build/launcher.raw $B/tfr911-level2-fn.t35
	python3 gen-rom-header.py build/launcher.raw $B/tfr911-level2-fn.t35 > $@

# The same disk, but booting from the FujiNet: its OS9Boot finds /DD on
# DriveWire drive 0 (/X0).  Put it where the FujiNet can mount it in disk
# slot 1 (its SD card, or a TNFS server), then type  l2fn  at TCL>.
build/tfr911-level2-fn.dsk: build/tfr911-level2.dsk $B/tfr911-level2-fn.o9b $B/tfr911-level2-fn.t35
	cp -f $< $@
	$(OS9_TOOL) gen -t'$B/tfr911-level2-fn.t35' -b'$B/tfr911-level2-fn.o9b' $@

# Update an existing disk (default: the one run-l2.sh uses) to this build:
# new OS9Boot, and every file in CMDS and SYS.  Other files are left alone.
# Quit the tether and press RESET on the board first.
DISK ?= level2.dsk
NEW = build/tfr911-level2.dsk
BOOTFILE ?= $B/tfr911-level2.o9b
install: $(NEW)
	$(OS9_TOOL) gen -b=$(BOOTFILE) $(DISK)
	for d in CMDS SYS; do \
	  $(OS9_TOOL) makdir $(DISK),$$d 2>/dev/null; \
	  for f in $$($(OS9_TOOL) dir $(NEW),$$d | awk 'NR > 2'); do \
	    $(OS9_TOOL) copy -r $(NEW),$$d/$$f $(DISK),$$d/$$f || exit 1; \
	    $(OS9_TOOL) attr -q -r -w -pr $(DISK),$$d/$$f; \
	    if [ $$d = CMDS ]; then $(OS9_TOOL) attr -q -e -pe $(DISK),$$d/$$f; fi; \
	  done; \
	done
	@echo "Updated $(DISK).  Press RESET on the board to boot the new OS9Boot."

# A copy of DISK (your files and all) that boots from the FujiNet:
#   make fujinet-disk [DISK=level2.dsk] [FNDISK=level2-fn.dsk]
# Then put FNDISK in the FujiNet's device slot 1 (see fujinet/README.md).
FNDISK ?= level2-fn.dsk
fujinet-disk: $(NEW) $B/tfr911-level2-fn.o9b
	cp -f $(DISK) $(FNDISK)
	$(MAKE) install DISK=$(FNDISK) BOOTFILE=$B/tfr911-level2-fn.o9b

# Copy story files to /DD/GAMES on a disk:  make stories STORIES="..." DISK=...
stories:
	@test -n "$(STORIES)" || { echo 'Say which: make stories STORIES="zork1.z3 ..."'; exit 1; }
	$(OS9_TOOL) makdir $(DISK),GAMES 2>/dev/null || true
	for f in $(STORIES); do \
	  $(OS9_TOOL) copy -r $$f $(DISK),GAMES/$$(basename $$f) || exit 1; \
	  $(OS9_TOOL) attr -q -r -w -pr $(DISK),GAMES/$$(basename $$f); \
	done
	$(OS9_TOOL) dir $(DISK),GAMES

# The reference manual (needs XeLaTeX, and the Times, Helvetica and
# Courier fonts that come with macOS).
manual: doc/tfr911h-reference.pdf
doc/tfr911h-reference.pdf: doc/manual.tex
	cd doc && for pass in 1 2; do \
	  xelatex -interaction=nonstopmode -halt-on-error -jobname=tfr911h-reference manual.tex >/dev/null || exit 1; \
	done
	cd doc && rm -f tfr911h-reference.aux tfr911h-reference.log tfr911h-reference.toc tfr911h-reference.out

clean:
	rm -rf build

FORCE:
.PHONY: all clean install fujinet-disk stories manual FORCE
