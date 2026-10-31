// sound.c -- chords for board events on the SN76489 the TFR/911H firmware
// has at $FF41 (three square-wave voices; see sn76489(4)).  They are the
// Atari's, as the MS-DOS client's AdLib plays them: three notes at once,
// dying away, and for some a second chord after the first.  Nothing
// blocks: snd_tick(), which every reading of the clock calls (input.c),
// turns the volume down a step at a time, starts what follows, and lets a
// chord go when its time is up.  A chord still sounding is cut off only
// by a higher-numbered one.
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#include "../platform.h"

#ifdef _CMOC_VERSION_
#define SND (*(unsigned char *)0xFF41)
#else
static unsigned char SND;       // testing on Unix: no sound chip
#endif

// Tone periods for the chip's 3.579545 MHz clock: 3579545 / 32 / Hz.
#define D3 762
#define E3 679
#define F3 641
#define G3 571
#define A3 508
#define B3 453
#define C4 428
#define D4 381
#define E4 339
#define F4 320
#define G4 285
#define A4 254
#define NONE 0xFF

// Entries 0-6 are platform.h's chords, the rest only follow on.
static const unsigned chords[11 * 3] = {
    D3, F3, A3,   C4, E4, G4,   C4, E4, G4,   B3, D4, F4,     // spike, buy, hive, hivejack
    E3, G3, B3,   G3, B3, D4,   F3, A3, C4,                   // Black ICE, turn, null-route
    C4, F4, A4,   C4, E4, G4,   C4, E4, G4,   B3, F4, A4      // after buy, turn, null-route
};
// How long each sounds, in sixtieths of a second (the AdLib's BIOS ticks
// times 3.3), and what follows it.
static const unsigned char lengths[11] = { 13, 16, 40, 40, 60, 16, 30, 16, 16, 30, 150 };
static const unsigned char follows[11] = { NONE, 7, NONE, NONE, NONE, 8, 9, NONE, NONE, 10, NONE };

static unsigned char on, sounding = NONE, prio, began, level;

static void tone(unsigned char voice, unsigned period)
{
    SND = (unsigned char)(0x80 | (voice << 5) | (period & 15));
    SND = (unsigned char)((period >> 4) & 0x3F);
}

// Attenuation: 0 is loudest, 15 is off.
static void volume(unsigned char att)
{
    unsigned char v;
    level = att;
    for (v = 0; v < 3; v++)
        SND = (unsigned char)(0x90 | (v << 5) | att);
}

void snd_off(void)
{
    volume(15);
    SND = 0xFF;                 // the noise voice off too
    sounding = NONE;
}

static unsigned char now;       // the clock, as snd_tick() last saw it

static void start(unsigned char c)
{
    const unsigned *p = chords + c * 3;
    unsigned char v;
    sounding = c;
    began = now;
    for (v = 0; v < 3; v++)
        tone(v, p[v]);
    volume(2);
}

void snd_init(void)
{
    on = 1;
    snd_off();
}

unsigned char snd_is_on(void)
{
    return on;
}

void snd_toggle(void)
{
    on ^= 1;
    if (on)
        play_chord(CHORD_HIVE);
    else
        snd_off();
}

void play_chord(unsigned char n)
{
    if (!on || (sounding != NONE && n < prio))
        return;
    prio = n;
    start(n);
}

// Called with each reading of the clock, which input.c keeps in `ticks`.
void snd_tick_at(unsigned char t)
{
    unsigned char gone, len, att;
    now = t;
    if (sounding == NONE)
        return;
    gone = (unsigned char)(t - began);
    len = lengths[sounding];
    if (gone >= len) {
        if (follows[sounding] != NONE)
            start(follows[sounding]);
        else
            snd_off();
        return;
    }
    att = (unsigned char)(2 + (unsigned)gone * 13 / len);
    if (att != level)
        volume(att);
}
