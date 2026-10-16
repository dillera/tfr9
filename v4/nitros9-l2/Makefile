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
OBJ_FLAGS = --6309 --format=obj --no-warn=ifp1 \
  --pragma=pcaspcr,nosymbolcase,condundefzero,undefextern,dollarnotlocal,noforwardrefmax,export

all: build/tfr911-level2.dsk ../tfr911h/nitros9_l2_rom.h

$B/Makefile: rmake-l2.tcl
	tclsh rmake-l2.tcl

$B/tfr911-level2.dsk: $B/Makefile FORCE
	rm -f $B/tfr911-level2.dsk
	$(MAKE) -C $B

build/tfr911-level2.dsk: $B/tfr911-level2.dsk startup build/grep build/helpmsg
	cp -f $< $@
	$(OS9_TOOL) copy -l -r startup $@,startup
	$(OS9_TOOL) attr -q -r -w -pr $@,startup
	$(OS9_TOOL) copy -r build/grep $@,CMDS/grep
	$(OS9_TOOL) attr -q -r -w -e -pr -pe $@,CMDS/grep
	$(OS9_TOOL) makdir $@,SYS
	$(OS9_TOOL) copy -l -r $(NITROS9_DIR)/level1/sys/errmsg $@,SYS/errmsg
	$(OS9_TOOL) copy -l -r build/helpmsg $@,SYS/helpmsg
	if ! $(OS9_TOOL) dir $@,CMDS | grep -qw basic09 && test -f $(B09DSK); then \
	  for f in basic09_6809:basic09 runb_6809:runb inkey:inkey syscall:syscall; do \
	    $(OS9_TOOL) copy -r $(B09DSK),CMDS/$${f%%:*} $@,CMDS/$${f##*:} && \
	    $(OS9_TOOL) attr -q -r -w -e -pr -pe $@,CMDS/$${f##*:} || exit 1; \
	  done; \
	fi

# grep is linked with NitrOS-9's alib, built here from its sources.
build/grep: $B/Makefile  # rmake-l2.tcl writes build/local/grep.as
	mkdir -p build/alib
	for f in $(NITROS9_DIR)/lib/alib/*.as; do \
	  $(LWASM) $(OBJ_FLAGS) -I$(NITROS9_DIR)/defs -I$(NITROS9_DIR)/lib/alib \
	    -o build/alib/$$(basename $$f .as).o $$f || exit 1; \
	done
	rm -f build/alib.a && $(LWAR) -c build/alib.a build/alib/*.o
	$(LWASM) $(OBJ_FLAGS) -I$(NITROS9_DIR)/defs -o build/sys.o $(NITROS9_DIR)/lib/sys6809l2.as
	$(LWASM) $(OBJ_FLAGS) -I$(NITROS9_DIR)/defs -o build/grep.o build/local/grep.as
	cd build && $(LWLINK) --format=os9 -o grep grep.o sys.o alib.a  # module name = output name

# NitrOS-9's help text, plus the Unix-style commands.
build/helpmsg: helpmsg.unix
	mkdir -p build
	cat $(NITROS9_DIR)/level2/coco3/sys/helpmsg helpmsg.unix > $@

build/launcher.raw: launcher.asm
	mkdir -p build
	$(or $(LWASM),lwasm) --format=raw -o'$@' '$<'

../tfr911h/nitros9_l2_rom.h: build/launcher.raw $B/tfr911-level2.dsk
	python3 gen-rom-header.py build/launcher.raw $B/tfr911-level2.t35 > $@

# Update an existing disk (default: the one run-l2.sh uses) to this build:
# new OS9Boot, and every file in CMDS and SYS.  Other files are left alone.
# Quit the tether and press RESET on the board first.
DISK ?= level2.dsk
NEW = build/tfr911-level2.dsk
install: $(NEW)
	$(OS9_TOOL) gen -b=$B/tfr911-level2.o9b $(DISK)
	for d in CMDS SYS; do \
	  $(OS9_TOOL) makdir $(DISK),$$d 2>/dev/null; \
	  for f in $$($(OS9_TOOL) dir $(NEW),$$d | awk 'NR > 2'); do \
	    $(OS9_TOOL) copy -r $(NEW),$$d/$$f $(DISK),$$d/$$f || exit 1; \
	    $(OS9_TOOL) attr -q -r -w -pr $(DISK),$$d/$$f; \
	    if [ $$d = CMDS ]; then $(OS9_TOOL) attr -q -e -pe $(DISK),$$d/$$f; fi; \
	  done; \
	done
	@echo "Updated $(DISK).  Press RESET on the board to boot the new OS9Boot."

clean:
	rm -rf build

FORCE:
.PHONY: all clean install FORCE
