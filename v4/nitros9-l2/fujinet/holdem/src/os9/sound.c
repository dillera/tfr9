/*
  Platform specific sound functions: the SN76489 the TFR/911H firmware has
  at $FF41, as the CoCo's Game Master Cartridge does (three square-wave
  voices and a noise voice; see sn76489(4)).  Each sound plays to its end
  before it returns, as upstream's do, a sixtieth of a second at a time.
  Keyclicks ($FF42 bit 0) are off while the game runs, and back as they
  were when it ends (soundDone()).
  @license gpl v. 3, see fujinet/LICENSE for details.
*/

#include "../misc.h"
#include "../platform-specific/sound.h"

#ifdef _CMOC_VERSION_
#ifndef FNHOLDEM_HOST
#define SND (*(volatile byte *)0xFF41)
#define SNDCTL (*(volatile byte *)0xFF42)
#endif
#endif
#ifndef SND
static byte SND, SNDCTL;    // testing on Unix: no sound chip
#endif

// Tone periods: 3579545 / 32 / Hz.
#define G4  285
#define C5  214
#define E5  170
#define G5  143
#define C6  107
#define E6  85

static byte saved_ctl;

static void tone(byte voice, word period)
{
    SND = (byte)(0x80 | (voice << 5) | (period & 15));
    SND = (byte)((period >> 4) & 0x3F);
}

// att: 0 is loudest, 15 is off.
static void volume(byte voice, byte att)
{
    SND = (byte)(0x90 | (voice << 5) | att);
}

// rate: 0-2, high to low.
static void noise(byte rate, byte att)
{
    SND = (byte)(0xE4 | rate);
    volume(3, att);
}

static void stopSound(void)
{
    byte v;
    for (v = 0; v < 4; v++)
        volume(v, 15);
}

static void note(word period, byte n, byte att)
{
    tone(0, period);
    volume(0, att);
    term_sleep(n);
}

void initSound()
{
    saved_ctl = SNDCTL;
    SNDCTL = (byte)(saved_ctl & ~1);
    stopSound();
}

// The keyclicks as they were, at the end
void soundDone(void)
{
    stopSound();
    SNDCTL = (byte)((SNDCTL & ~1) | (saved_ctl & 1));
}

void soundJoinGame()
{
    note(C5, 4, 5);
    note(E5, 4, 5);
    note(G5, 4, 5);
    note(C6, 8, 5);
    stopSound();
}

// Two chimes: the move is yours
void soundMyTurn()
{
    note(E6, 3, 4);
    volume(0, 15);
    term_sleep(3);
    note(C6, 8, 5);
    stopSound();
}

void soundGameDone()
{
    note(G4, 5, 5);
    note(C5, 5, 5);
    note(E5, 5, 5);
    note(G5, 12, 4);
    stopSound();
}

// A card flicked onto the felt
void soundDealCard()
{
    noise(0, 5);
    term_sleep(1);
    noise(1, 9);
    term_sleep(1);
    stopSound();
}

void soundTick()
{
    note(60, 1, 9);
    stopSound();
}

void soundPlayerJoin()
{
    note(C5, 4, 6);
    note(G5, 6, 6);
    stopSound();
}

void soundPlayerLeft()
{
    note(G5, 4, 6);
    note(C5, 6, 6);
    stopSound();
}

void soundSelectMove()
{
    note(E5, 3, 6);
    note(C6, 4, 6);
    stopSound();
}

void soundCursor()
{
    note(140, 1, 10);
    stopSound();
}

void soundCursorInvalid()
{
    note(900, 3, 8);
    stopSound();
}

// A chip clinks into the pot, higher for each player
void soundTakeChip(uint16_t counter)
{
    note((word)(70 - (word)(counter & 7) * 5), 2, 6);
    noise(0, 10);
    term_sleep(2);
    stopSound();
}
