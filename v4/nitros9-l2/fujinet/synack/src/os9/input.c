// input.c -- keys and the clock for the TFR/911H, and the way out.
//
// Keys come raw from the tether's terminal (term.c): Enter, Backspace,
// Esc (^C as well) as the game wants them.  The other platforms' spare
// keys are these:
//
//   Tab           move hints on and off (the Atari's OPTION)
//   ?             the instructions (HELP)
//   ^S            sound on and off
//   Left Right    turn the instructions' pages, as < and > do
//
// When no key is waiting, in_key() sleeps until the next tick, so the game
// loop goes round 60 times a second and leaves the 6309 to the others
// between.
//
// The clock is NitrOS-9's: D.Sec and D.Tick, which counts down the ticks
// of each second, read from the system's direct page with F$CpyMem, and
// counted on here, as the game wants a counter that only goes round.
// Each reading also moves the sound on (sound.c).
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#include "term.h"
#include "screen.h"
#include "../platform.h"

void snd_tick_at(unsigned char t);  // sound.c
void snd_off(void);
void display_sound(void);       // display.c

static unsigned count;          // ticks since the program started (it wraps)

#ifdef SAW_DEBUG
void debug_mark(char c);
#define MARK(c) debug_mark(c)
#else
#define MARK(c)
#endif

unsigned char in_key(void)
{
    unsigned char k;
    MARK('k');
#ifdef SAW_DEBUG
    {
        unsigned char err = 0;
        asm {
            clra
            ldb     #1              ; SS.Ready
            os9     $8D             ; I$GetStt
            bcc     @ok
            stb     :err
@ok
        }
        MARK(err ? 'x' : 'R');
    }
#endif
    k = term_inkey();
    MARK('K');
    switch (k) {
    case 0:
        MARK('s');
        term_sleep(1);
        MARK('S');
        return 0;
    case KEY_BREAK:
        return K_ESC;
    case KEY_LEFT:
    case KEY_PGUP:
        return '<';
    case KEY_RIGHT:
    case KEY_PGDN:
        return '>';
    case KEY_BS:
        return K_DELETE;
    case '\t':
        return K_OPTION;
    case 0x13:                  // ^S
        snd_toggle();
        display_sound();
        return 0;
    case '?':
        return K_HELP;
    case '\n':
        return K_RETURN;
    }
    if (k >= 'a' && k <= 'z')
        k -= 'a' - 'A';
    return k;
}

unsigned char ticks_per_sec(void)
{
    return 60;
}

#ifdef _CMOC_VERSION_

static unsigned char sysproc[512];      // the system process's descriptor
static unsigned char have_dat;
static unsigned last;                   // ticks into the minute, last time

static void os9_gprdsc(unsigned char id, unsigned char *buf)
{
    asm {
        lda     :id
        ldx     :buf
        os9     $18             ; F$GPrDsc
    }
}

static void os9_cpymem(unsigned char *dat, unsigned addr, unsigned n, unsigned char *dst)
{
    asm {
        pshs    u,y
        ldd     :dat
        ldx     :addr
        ldy     :n
        ldu     :dst
        os9     $1B             ; F$CpyMem
        puls    u,y
    }
}

unsigned char ticks(void)
{
    unsigned char t[2];         // D.Sec, D.Tick (60 down to 1)
    unsigned now;
    if (!have_dat) {
        MARK('p');
        os9_gprdsc(1, sysproc);
        have_dat = 1;
    }
    MARK('c');
    os9_cpymem(sysproc + 0x40, 0x2D, 2, t);         // P$DATImg; Level 2's D.Sec, D.Tick
    MARK('C');
    if (t[0] > 59 || t[1] > 60)
        t[0] = t[1] = 0;
    now = (unsigned)t[0] * 60 + (60 - t[1]);
    count += now >= last ? now - last : now + 3600 - last;
    last = now;
    snd_tick_at((unsigned char)count);
    return (unsigned char)count;
}

#else // Unix, for testing

#include <sys/time.h>

unsigned char ticks(void)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    count = (unsigned)((unsigned long)tv.tv_sec * 60 + tv.tv_usec / 16667);
    snd_tick_at((unsigned char)count);
    return (unsigned char)count;
}

#endif

void display_restore(void);     // display.c

void platform_quit(void)
{
    snd_off();
    display_restore();
    exit(0);
}
