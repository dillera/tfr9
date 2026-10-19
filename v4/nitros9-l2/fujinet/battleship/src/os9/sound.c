/*
  Platform specific sound functions: the SN76489 the TFR/911H firmware has
  at $FF41, as the CoCo's Game Master Cartridge does (three square-wave
  voices and a noise voice; see sn76489(4)).  Each sound plays to its end
  before it returns, as upstream's do, a sixtieth of a second at a time.
  Keyclicks ($FF42 bit 0) are off while the game runs, but for typing a
  name.
  @license gpl v. 3, see fujinet/LICENSE for details.
*/

#include "../misc.h"

#ifdef _CMOC_VERSION_
#define SND (*(volatile byte *)0xFF41)
#define SNDCTL (*(volatile byte *)0xFF42)
#else
static byte SND, SNDCTL;    // testing on Unix: no sound chip
#endif

// Tone periods: 3579545 / 32 / Hz.
#define C4  428
#define G4  285
#define C5  214
#define E5  170
#define G5  143
#define C6  107

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

// rate: 0-2, high to low, or 3, voice 2's tone.
static void noise(byte rate, byte att)
{
    SND = (byte)(0xE4 | rate);
    volume(3, att);
}

static void ticks(byte n)
{
    term_sleep(n);
}

void stopSound()
{
    byte v;
    for (v = 0; v < 4; v++)
        volume(v, 15);
}

static void note(word period, byte n, byte att)
{
    tone(0, period);
    volume(0, att);
    ticks(n);
}

void initSound()
{
    saved_ctl = SNDCTL;
    SNDCTL = (byte)(saved_ctl & ~1);
    stopSound();
}

void soundStop()
{
    stopSound();
}

// Keyclicks while a name is typed, if they were on to begin with.
void enableKeySounds()
{
    SNDCTL = (byte)(SNDCTL | (saved_ctl & 1));
}

void disableKeySounds()
{
    SNDCTL = (byte)(SNDCTL & ~1);
}

void soundCursor()
{
    if (prefs.disableSound)
        return;
    note(140, 1, 10);
    stopSound();
}

void soundTick()
{
    if (prefs.disableSound)
        return;
    note(60, 1, 9);
    stopSound();
}

void soundSelect()
{
    if (prefs.disableSound)
        return;
    note(E5, 3, 6);
    note(C6, 4, 6);
    stopSound();
}

void soundJoinGame()
{
    if (prefs.disableSound)
        return;
    note(C5, 4, 5);
    note(E5, 4, 5);
    note(G5, 4, 5);
    note(C6, 8, 5);
    stopSound();
}

// Sonar: a ping, and its echo.
void soundMyTurn()
{
    byte a;
    if (prefs.disableSound)
        return;
    tone(0, 85);
    for (a = 1; a < 15; a++) {
        volume(0, a);
        ticks(2);
    }
    for (a = 8; a < 15; a++) {
        volume(0, a);
        ticks(2);
    }
    stopSound();
}

void soundGameDone()
{
    if (prefs.disableSound)
        return;
    note(G4, 6, 4);
    note(C5, 6, 4);
    note(E5, 6, 4);
    note(G5, 14, 4);
    note(E5, 4, 4);
    note(G5, 24, 3);
    stopSound();
}

// A thump, as a ship goes in the water.
void soundPlaceShip()
{
    byte a;
    if (prefs.disableSound)
        return;
    tone(1, C4 * 2);
    volume(1, 5);
    for (a = 3; a < 15; a += 2) {
        noise(1, a);
        ticks(1);
    }
    stopSound();
}

// A shell's whistle, falling.
void soundAttack()
{
    word p;
    if (prefs.disableSound)
        return;
    volume(0, 7);
    for (p = 50; p < 250; p += 12) {
        tone(0, p);
        ticks(1);
    }
    stopSound();
}

void soundInvalid()
{
    if (prefs.disableSound)
        return;
    tone(0, 1017);
    tone(1, 960);
    volume(0, 4);
    volume(1, 4);
    ticks(10);
    stopSound();
}

// An explosion: noise at the pitch of voice 2 (silent), sweeping down.
static void boom(byte len, word period)
{
    byte i;
    tone(2, period);
    volume(2, 15);
    noise(3, 0);
    for (i = 0; i < len; i++) {
        tone(2, period += 24);
        // (word): CMOC keeps a byte times a byte constant to a byte
        volume(3, (byte)((word)i * 15 / len));
        ticks(1);
    }
    stopSound();
}

void soundHit()
{
    if (prefs.disableSound)
        return;
    boom(30, 120);
}

// A ship going down: a longer boom, over a low rumble.
void soundSink()
{
    if (prefs.disableSound)
        return;
    tone(1, 1000);
    volume(1, 5);
    boom(55, 200);
}

// A splash: hiss, falling.
void soundMiss()
{
    byte i;
    if (prefs.disableSound)
        return;
    for (i = 0; i < 12; i++) {
        noise((byte)(i / 5), (byte)(2 + i));
        ticks(1);
    }
    stopSound();
}
