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
# Games are C, compiled with CMOC (http://perso.b2b2c.ca/~sarrazip/dev/cmoc.html),
# and so are vi, info and tsh (the shell with line editing, in tsh/).
CMOC ?= cmoc
GAMES = $(if $(shell command -v $(CMOC)),build/tetris build/zm build/vi build/info build/tsh)
# The FujiNet commands (see fujinet/README.md), and the FujiNet programs
# in C, which need CMOC too; and splash, the screen startup shows, which
# asks the FujiNet for its address.
FN_CMDS = $(patsubst fujinet/cmds/%.as,build/fn/%,$(wildcard fujinet/cmds/*.as))
FN_APPS = $(if $(shell command -v $(CMOC)),build/fn/fnnews build/fn/fnstocks build/fn/fnbounce \
  build/fn/fnweather build/fn/fnbattleship build/fn/synack build/fn/splash)
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

build/tfr911-level2.dsk: $B/tfr911-level2.dsk startup tsh/tshrc build/grep build/helpmsg $(GAMES) $(FN_CMDS) $(FN_APPS) \
  build/fn/synack.hlp
	cp -f $< $@
	$(OS9_TOOL) copy -l -r startup $@,startup
	$(OS9_TOOL) attr -q -r -w -pr $@,startup
	$(OS9_TOOL) copy -r build/grep $@,CMDS/grep
	$(OS9_TOOL) attr -q -r -w -e -pr -pe $@,CMDS/grep
	for g in $(GAMES) $(FN_CMDS) $(FN_APPS); do \
	  $(OS9_TOOL) copy -r $$g $@,CMDS/$$(basename $$g) && \
	  $(OS9_TOOL) attr -q -r -w -e -pr -pe $@,CMDS/$$(basename $$g) || exit 1; \
	done
	@test -n "$(GAMES)" || echo "NOTE: $(CMOC) not found, so no games on the disk."
	$(if $(STORIES),$(MAKE) stories DISK=$@)
	$(OS9_TOOL) makdir $@,SYS
	$(OS9_TOOL) copy -l -r $(NITROS9_DIR)/level1/sys/errmsg $@,SYS/errmsg
	$(OS9_TOOL) copy -l -r build/helpmsg $@,SYS/helpmsg
	$(OS9_TOOL) copy -l -r tsh/tshrc $@,SYS/tshrc
	$(OS9_TOOL) copy -l -r build/fn/synack.hlp $@,SYS/synack.hlp
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
# (No nosymbolcase here: fnwx and fnconfig have labels that differ
# only in case, as in NitrOS-9's own rules for linked commands.)
# ANSI=1: fnconfig and fnwx draw with ANSI sequences, for the
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

# fujinet-lib for NitrOS-9: FujiNetWIFI/fujinet-lib's common code with a
# bus for OS-9, which reaches the FujiNet through SS.Fuji (see
# fujinet/fujinet-lib/README.md).  CMOC is given an ISO C preprocessor
# (cmoc-cpp), since fujinet-lib indents its directives and macOS's cpp
# is a traditional one.
FNLIB = fujinet/fujinet-lib
FNLIB_CFLAGS = --os9 --cpp=$(CURDIR)/$(FNLIB)/cmoc-cpp -I$(FNLIB)/include
FNLIB_SRC = $(wildcard $(FNLIB)/common/*.c $(FNLIB)/bus/os9/*.c $(FNLIB)/bus/os9/*.s)
FNLIB_OBJS = $(patsubst $(FNLIB)/%,build/fnlib/%.o,$(FNLIB_SRC))
FNLIB_A = build/fnlib/libfujinet.a
build/fnlib/%.o: $(FNLIB)/% $(wildcard $(FNLIB)/include/*.h $(FNLIB)/bus/os9/*.h)
	mkdir -p $(dir $@)
	$(CMOC) $(FNLIB_CFLAGS) -I$(FNLIB)/bus/os9 -c -o $@ $<
$(FNLIB_A): $(FNLIB_OBJS)
	rm -f $@ && $(LWAR) -a -r $@ $^
fujinet-lib: $(FNLIB_A)

# The FujiNet programs in C, linked with fujinet-lib.  fujinet/c is their
# terminal.  The stack space counts only when compiling the file with main().
FN_C = fujinet/c
FN_CFLAGS = $(FNLIB_CFLAGS) --function-stack=0 --add-os9-stack-space=2048 \
  -DBUILD_OS9 -I$(FN_C)
FN_C_OBJS = build/fn/c/term.o
build/fn/c/%.o: $(FN_C)/%.c $(wildcard $(FN_C)/*.h)
	mkdir -p $(dir $@)
	$(CMOC) $(FN_CFLAGS) -c -o $@ $<

# fnnews: FujiNetWIFI/fujinet-news's CoCo client.
NEWS = fujinet/news
NEWS_OBJS = $(patsubst $(NEWS)/%.c,build/fn/news/%.o,$(wildcard $(NEWS)/*.c))
build/fn/news/%.o: $(NEWS)/%.c $(wildcard $(NEWS)/*.h $(FN_C)/*.h)
	mkdir -p $(dir $@)
	$(CMOC) $(FN_CFLAGS) -c -o $@ $<
build/fn/fnnews: $(NEWS_OBJS) $(FN_C_OBJS) $(FNLIB_A)
	cd build/fn && $(CMOC) --os9 -o fnnews $(patsubst build/fn/%,%,$(NEWS_OBJS) $(FN_C_OBJS)) \
	  -L../fnlib -lfujinet

# fnstocks: FujiNetWIFI/fujinet-stocks, commit STOCKS_VERSION, with an
# os9 platform beside its coco one.
STOCKS = fujinet/stocks
STOCKS_VERSION = 1ececd8
STOCKS_OBJS = $(patsubst $(STOCKS)/src/%.c,build/fn/stocks/%.o,\
  $(wildcard $(STOCKS)/src/*.c $(STOCKS)/src/os9/*.c))
STOCKS_CFLAGS = $(FN_CFLAGS) -I$(STOCKS)/include -I$(STOCKS)/include/os9 \
  '-DGIT_VERSION="$(STOCKS_VERSION)"'
build/fn/stocks/%.o: $(STOCKS)/src/%.c \
    $(wildcard $(STOCKS)/include/*.h $(STOCKS)/include/os9/*.h $(FN_C)/*.h)
	mkdir -p $(dir $@)
	$(CMOC) $(STOCKS_CFLAGS) -c -o $@ $<
build/fn/fnstocks: $(STOCKS_OBJS) $(FN_C_OBJS) $(FNLIB_A)
	cd build/fn && $(CMOC) --os9 -o fnstocks $(patsubst build/fn/%,%,$(STOCKS_OBJS) $(FN_C_OBJS)) \
	  -L../fnlib -lfujinet

# fnweather: the weather, after FujiNetWIFI/fujinet-weather's CoCo client
# (fujinet/cmds/fnwx.as is the first fnweather, in assembly).
WEATHER = fujinet/weather
WEATHER_OBJS = $(patsubst $(WEATHER)/%.c,build/fn/weather/%.o,$(wildcard $(WEATHER)/*.c))
build/fn/weather/%.o: $(WEATHER)/%.c $(wildcard $(WEATHER)/*.h $(FN_C)/*.h)
	mkdir -p $(dir $@)
	$(CMOC) $(FN_CFLAGS) -c -o $@ $<
build/fn/fnweather: $(WEATHER_OBJS) $(FN_C_OBJS) $(FNLIB_A)
	cd build/fn && $(CMOC) --os9 -o fnweather $(patsubst build/fn/%,%,$(WEATHER_OBJS) $(FN_C_OBJS)) \
	  -L../fnlib -lfujinet

# fnbounce: markjfisher/bounce-world-client, commit BOUNCE_VERSION, with an
# os9 platform beside its coco one.
BOUNCE = fujinet/bounce
BOUNCE_VERSION = 5ac9408
BOUNCE_OBJS = $(patsubst $(BOUNCE)/src/%.c,build/fn/bounce/%.o,\
  $(BOUNCE)/src/main.c $(wildcard $(BOUNCE)/src/common/*.c $(BOUNCE)/src/os9/*.c))
BOUNCE_CFLAGS = $(FN_CFLAGS) -I$(BOUNCE)/src/include -I$(BOUNCE)/src/os9
build/fn/bounce/%.o: $(BOUNCE)/src/%.c \
    $(wildcard $(BOUNCE)/src/include/*.h $(BOUNCE)/src/os9/*.h $(FN_C)/*.h)
	mkdir -p $(dir $@)
	$(CMOC) $(BOUNCE_CFLAGS) -c -o $@ $<
# version.o, with the date of the build and the upstream commit for the
# title screen, is compiled again whenever anything else in fnbounce is.
BOUNCE_VERSION_O = build/fn/bounce/os9/version.o
$(BOUNCE_VERSION_O): $(filter-out $(BOUNCE_VERSION_O),$(BOUNCE_OBJS)) $(FN_C_OBJS) $(FNLIB_A)
$(BOUNCE_VERSION_O): BOUNCE_CFLAGS += '-DBUILD_DATE="$(shell date +%Y-%m-%d)"' \
  '-DUPSTREAM_VERSION="$(BOUNCE_VERSION)"'
build/fn/fnbounce: $(BOUNCE_OBJS) $(FN_C_OBJS) $(FNLIB_A)
	cd build/fn && $(CMOC) --os9 -o fnbounce $(patsubst build/fn/%,%,$(BOUNCE_OBJS) $(FN_C_OBJS)) \
	  -L../fnlib -lfujinet

# fnbattleship: FujiNetWIFI/fujinet-battleship, commit BATTLESHIP_VERSION,
# with an os9 platform beside its coco one.  src/os9/shipart.h is made by
# src/os9/shipart.py.
BATTLESHIP = fujinet/battleship
BATTLESHIP_VERSION = 87b8cc7
BATTLESHIP_OBJS = $(patsubst $(BATTLESHIP)/src/%.c,build/fn/battleship/%.o,\
  $(wildcard $(BATTLESHIP)/src/*.c $(BATTLESHIP)/src/os9/*.c))
build/fn/battleship/%.o: $(BATTLESHIP)/src/%.c \
    $(wildcard $(BATTLESHIP)/src/*.h $(BATTLESHIP)/src/*/*.h $(FN_C)/*.h)
	mkdir -p $(dir $@)
	$(CMOC) $(FN_CFLAGS) -c -o $@ $<
BATTLESHIP_VERSION_O = build/fn/battleship/os9/version.o
$(BATTLESHIP_VERSION_O): $(filter-out $(BATTLESHIP_VERSION_O),$(BATTLESHIP_OBJS)) $(FN_C_OBJS) $(FNLIB_A)
$(BATTLESHIP_VERSION_O): FN_CFLAGS += '-DBUILD_DATE="$(shell date +%Y-%m-%d)"' \
  '-DUPSTREAM_VERSION="$(BATTLESHIP_VERSION)"'
build/fn/fnbattleship: $(BATTLESHIP_OBJS) $(FN_C_OBJS) $(FNLIB_A)
	cd build/fn && $(CMOC) --os9 -o fnbattleship \
	  $(patsubst build/fn/%,%,$(BATTLESHIP_OBJS) $(FN_C_OBJS)) -L../fnlib -lfujinet

# synack: SYN-ACK WIRE, the client of dillera/syn-ack-wire (commit
# SYNACK_VERSION; michaelsternberg's game), with an os9 platform: its
# portable code (src/common) as it is, and the TFR/911H's screen, keys,
# sound and network in src/os9.  The instructions go in SYS/synack.hlp.
SYNACK = fujinet/synack
SYNACK_VERSION = 445c0c9
SYNACK_SRC = $(SYNACK)/src/common/main.c $(SYNACK)/src/common/game.c $(SYNACK)/src/common/proto.c \
  $(wildcard $(SYNACK)/src/os9/*.c)
SYNACK_OBJS = $(patsubst $(SYNACK)/src/%.c,build/fn/saw/%.o,$(SYNACK_SRC))
SYNACK_DEFS = -D__fastcall__= -DDISPLAY_IDLE=1 -DTITLE_IDLE=1 -DQUIT_TO_OS=1 \
  '-DQUIT_ASK="QUIT TO OS-9? Y/N"' $(if $(SYNACK_URL),'-DURL="$(SYNACK_URL)"')
SYNACK_HDRS = $(wildcard $(SYNACK)/src/*.h $(SYNACK)/src/common/*.h $(SYNACK)/src/os9/*.h $(FN_C)/*.h)
build/fn/saw/%.o: $(SYNACK)/src/%.c $(SYNACK_HDRS)
	mkdir -p $(dir $@)
	$(CMOC) $(FN_CFLAGS) -I$(SYNACK)/src/os9/include $(SYNACK_DEFS) -c -o $@ $<
build/fn/synack: $(SYNACK_OBJS) $(FN_C_OBJS) $(FNLIB_A)
	cd build/fn && $(CMOC) --os9 -o synack $(patsubst build/fn/%,%,$(SYNACK_OBJS) $(FN_C_OBJS)) \
	  -L../fnlib -lfujinet
$(SYNACK)/src/os9/title.h: $(SYNACK)/src/os9/title.py $(SYNACK)/src/atari/title4.atascii
	python3 $(SYNACK)/src/os9/title.py $(SYNACK)/src/atari/title4.atascii > $@
build/fn/synack.hlp: $(sort $(wildcard $(SYNACK)/help/HELP*.TXT))
	mkdir -p build/fn
	for f in $^; do awk '{ print } END { for (i = NR; i < 24; i++) print "" }' $$f; done > $@

# splash: the boot screen.  Its picture, splash/art.h, is made by
# splash/art.py.  build/splash-host shows it here, with made-up values.
splash/art.h: splash/art.py
	python3 splash/art.py > $@
build/fn/splash.o: splash/splash.c splash/art.h
	mkdir -p build/fn
	$(CMOC) $(FN_CFLAGS) -c -o $@ $<
build/fn/splash: build/fn/splash.o $(FNLIB_A)
	cd build/fn && $(CMOC) --os9 -o splash splash.o -L../fnlib -lfujinet
build/splash-host: splash/splash.c splash/art.h
	mkdir -p build
	$(CC) -O2 -Wall -o $@ splash/splash.c

# fnnews and fnstocks for this computer, for testing: curl and python3 are
# the FujiNet (fujinet/c/host/fujinet-host.c, with the few fujinet-lib
# calls they use).
FN_HOST = $(FN_C)/host
build/fnnews-host: $(wildcard $(NEWS)/*.[ch] $(FN_C)/*.[ch] $(FN_HOST)/*.[ch])
	mkdir -p build
	$(CC) -O2 -I$(FN_C) -I$(FN_HOST) -o $@ $(NEWS)/*.c $(FN_C)/term.c $(FN_HOST)/fujinet-host.c
build/fnweather-host: $(wildcard $(WEATHER)/*.[ch] $(FN_C)/*.[ch] $(FN_HOST)/*.[ch])
	mkdir -p build
	$(CC) -O2 -I$(FN_C) -I$(FN_HOST) -o $@ $(WEATHER)/*.c $(FN_C)/term.c $(FN_HOST)/fujinet-host.c
build/fnstocks-host: $(wildcard $(STOCKS)/src/*.c $(STOCKS)/src/os9/*.c \
    $(STOCKS)/include/*.h $(STOCKS)/include/os9/*.h $(FN_C)/*.[ch] $(FN_HOST)/*.[ch])
	mkdir -p build
	$(CC) -O2 -DBUILD_OS9 '-DGIT_VERSION="$(STOCKS_VERSION)"' -I$(STOCKS)/include \
	  -idirafter $(STOCKS)/include/os9 -I$(FN_C) -I$(FN_HOST) -o $@ $(STOCKS)/src/*.c \
	  $(STOCKS)/src/os9/conio.c $(STOCKS)/src/os9/init.c $(FN_C)/term.c $(FN_HOST)/fujinet-host.c

# fnbounce for this computer, for testing against a server: compiled as
# for CMOC, with fujinet/bounce/host's cmoc.h and coco.h, so that it takes
# the same paths; term.c's Unix side, and fujinet-host.c's TCP.
BOUNCE_HOST = $(BOUNCE)/host
build/fnbounce-host: $(wildcard $(BOUNCE)/src/*.c $(BOUNCE)/src/*/*.[ch] \
    $(BOUNCE_HOST)/*.h $(FN_C)/*.[ch] $(FN_HOST)/*.[ch])
	mkdir -p build/bounce-host
	for f in $(BOUNCE)/src/main.c $(BOUNCE)/src/common/*.c $(BOUNCE)/src/os9/*.c; do \
	  $(CC) -O2 -w -D_CMOC_VERSION_ -DBUILD_OS9 -DBWC_HOST '-DBUILD_DATE="$(shell date +%Y-%m-%d)"' \
	    '-DUPSTREAM_VERSION="$(BOUNCE_VERSION)"' -I$(BOUNCE_HOST) -I$(FN_HOST) -I$(FN_C) \
	    -I$(BOUNCE)/src/include -I$(BOUNCE)/src/os9 \
	    -c -o build/bounce-host/$$(echo $$f | tr / _).o $$f || exit 1; \
	done
	$(CC) -O2 -I$(FN_C) -I$(FN_HOST) -o $@ build/bounce-host/*.o $(FN_C)/term.c $(FN_HOST)/fujinet-host.c

# fnbattleship for this computer, for testing against the server.
# SYN-ACK WIRE here, against a server over TCP: build/synack-host, with
# SYNACK_HLP naming the instructions (build/fn/synack.hlp).
# make build/synack-host SYNACK_URL=N:TCP://localhost:6502/ for your own.
build/synack-host: $(SYNACK_SRC) $(SYNACK_HDRS) $(FN_C)/term.c $(FN_HOST)/fujinet-host.c build/fn/synack.hlp
	mkdir -p build
	$(CC) -O2 -w $(SYNACK_DEFS) -I$(FN_C) -I$(FN_HOST) -o $@ $(SYNACK_SRC) \
	  $(FN_C)/term.c $(FN_HOST)/fujinet-host.c
# ...and playing by itself (the upstream test/autoplay.c), for testing.
build/synack-auto: $(SYNACK_SRC) $(SYNACK_HDRS) $(SYNACK)/test/autoplay.c $(FN_C)/term.c $(FN_HOST)/fujinet-host.c
	mkdir -p build
	$(CC) -O2 -w $(SYNACK_DEFS) -DAUTOPLAY -DDUMP_TICKS=0 '-DAUTO_HANDLE="$(or $(AUTO_HANDLE),TFR)"' \
	  -I$(FN_C) -I$(FN_HOST) -o $@ $(SYNACK_SRC) $(SYNACK)/test/autoplay.c \
	  $(FN_C)/term.c $(FN_HOST)/fujinet-host.c

build/fnbattleship-host: $(wildcard $(BATTLESHIP)/src/*.[ch] $(BATTLESHIP)/src/*/*.[ch] \
    $(FN_C)/*.[ch] $(FN_HOST)/*.[ch])
	mkdir -p build
	$(CC) -O2 -w -DBUILD_OS9 '-DBUILD_DATE="$(shell date +%Y-%m-%d)"' \
	  '-DUPSTREAM_VERSION="$(BATTLESHIP_VERSION)"' -I$(FN_C) -I$(FN_HOST) -o $@ \
	  $(BATTLESHIP)/src/*.c $(BATTLESHIP)/src/os9/*.c $(FN_C)/term.c $(FN_HOST)/fujinet-host.c

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
build/tsh: tsh/tsh.c
	mkdir -p build
	cd build && $(CMOC) --os9 --function-stack=0 --add-os9-stack-space=3072 -o tsh ../tsh/tsh.c
# tsh for this computer, for testing: commands go to /bin/sh, and
# TSH_EXECDIR, TSH_HISTORY and TSH_RC stand in for the OS-9 places.
build/tsh-host: tsh/tsh.c
	mkdir -p build
	$(CC) -O2 -Wall -o $@ tsh/tsh.c

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
	@$(OS9_TOOL) list $(DISK),startup 2>/dev/null | grep -qw tsh || \
	  echo "To start tsh when booting, add the line \"tsh </term\" to $(DISK)'s startup."
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

# The TFR Manual and Reference (needs XeLaTeX, and the Times, Helvetica
# and Courier fonts that come with macOS).
manual: doc/tfr-manual.pdf
doc/tfr-manual.pdf: doc/manual.tex
	cd doc && for pass in 1 2; do \
	  xelatex -interaction=nonstopmode -halt-on-error -jobname=tfr-manual manual.tex >/dev/null || exit 1; \
	done
	cd doc && rm -f tfr-manual.aux tfr-manual.log tfr-manual.toc tfr-manual.out

# The TFR Installation Guide, for a release (the same fonts).
install-guide: doc/tfr-install-guide.pdf
doc/tfr-install-guide.pdf: doc/install.tex
	cd doc && for pass in 1 2; do \
	  xelatex -interaction=nonstopmode -halt-on-error -jobname=tfr-install-guide install.tex >/dev/null || exit 1; \
	done
	cd doc && rm -f tfr-install-guide.aux tfr-install-guide.log tfr-install-guide.toc \
	  tfr-install-guide.out tfr-install-guide.lof tfr-install-guide.lot

clean:
	rm -rf build

FORCE:
.PHONY: all clean install fujinet-disk fujinet-lib stories manual install-guide FORCE
