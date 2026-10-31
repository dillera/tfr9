/* Test hooks for an unattended client (make test builds build/atari/synack-auto.xex):
 * auto_key() stands in for in_key(): it types a handle, then presses keys a
 * player might press for each prompt, wrong ones included, so the local checks
 * and the server's refusals both get exercised. When a new prompt opens, and
 * every DUMP_TICKS, it executes BRK; atari800 -bbrk stops in its monitor there, and tools/run-test.sh feeds
 * the monitor commands that save screen memory to a file. On a PC (make pc-apple2) it calls pc_dump()
 * instead, which saves the screen array; on the NES, where a BRK would only go to the IRQ handler, it
 * calls test/nes_dump.c's, which the emulator watches (tools/nes.lua). */
#include "../src/platform.h"
#include "../src/common/game.h"

#ifndef DUMP_TICKS
#define DUMP_TICKS 240             /* 0: no screen dumps */
#endif
#ifndef KEY_TICKS
#define KEY_TICKS 20               /* between keys: a third of a second at 60 ticks a second */
#endif

static unsigned char seed = 0x5A, last, prev, shown = 0xFF, typed, paged;  /* 0xFF: dump the title too */
static unsigned elapsed;
#ifndef AUTO_HANDLE
#define AUTO_HANDLE "ATARI"
#endif
static const char name[] = AUTO_HANDLE "\r";
/* first, the help pages: open, page to the end (extra presses are ignored), back one, leave */
static const unsigned char paging[] = { K_HELP, '>', '>', '>', '>', '>', '>', '>', '>', '<', K_ESC, 0 };

#if defined(__CC65__) && !defined(__NES__)
#define DUMP() do { __asm__ ("brk"); __asm__ ("nop"); } while (0)
#else
void pc_dump(void);                         /* test/pc_harness.c, test/nes_dump.c */
#define DUMP() pc_dump()
#endif

static unsigned char rnd(void)
{
    seed = (seed << 1) ^ ((seed & 0x80) ? 0x1D : 0);   /* 8-bit LFSR */
    return seed;
}

unsigned char auto_key(void)
{
    unsigned char t = ticks(), r;
    elapsed += (unsigned char)(t - prev);
    prev = t;
    if (!status_len && !status_code && shown == 0xFF)
        return 0;                                   /* the title, until the server answers: its status shows */
    if (DUMP_TICKS && (elapsed >= DUMP_TICKS || pr_kind != shown)) {
        elapsed = 0;
        shown = pr_kind;
        DUMP();
    }
    in_key();
    if ((unsigned char)(t - last) < KEY_TICKS)
        return 0;
    last = t;
    if (paging[paged]) {                            /* a dump before each key shows each page */
        if (DUMP_TICKS)
            DUMP();
        return paging[paged++];
    }
    if (name[typed])                                /* title: the handle, then Return */
        return name[typed++];
    r = rnd();
    switch (pr_kind) {
    case 'S':
        return '3';
    case 'P':
        return '1' + (r & 7);                       /* 7 and 8 are ignored */
    case 'F':
    case 'K':
        return syn_letter[r % NSYN];
    case 'D':
        return (r & 3) ? '0' + (r & 3) : K_RETURN;
    case 'B':
        if (r < 40)
            return K_DELETE;
        if (r < 100)
            return K_RETURN;
        if (r < 120)
            return 'N';
        return syn_letter[r % NSYN];
    }
    return 0;
}
