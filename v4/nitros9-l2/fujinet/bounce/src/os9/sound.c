#include "conio_wrapper.h"
#include "sound.h"

// The SN76489 the TFR/911H firmware has at $FF41, as the CoCo's Game
// Master Cartridge does: a collision is a burst of noise on its noise
// voice, which sound_tick() fades as the main loop goes round.

#ifdef BWC_HOST
static byte SND;            // testing on Unix: no sound chip
#else
#define SND (*(volatile byte *)0xFF41)
#endif

static byte fade = 15;      // the noise voice's attenuation: 15 is off

void init_sound() {
    stop_sound();
}

void stop_sound() {
    SND = 0x9F;     // each voice off
    SND = 0xBF;
    SND = 0xDF;
    SND = 0xFF;
    fade = 15;
}

void sound_collision() {
    SND = 0xE4;     // noise: white, the highest rate
    fade = 0;
    SND = 0xF0;     // the noise voice at full volume
}

void sound_tick() {
    if (fade >= 15)
        return;
    fade += 3;
    if (fade > 15)
        fade = 15;
    SND = (byte)(0xF0 | fade);
}
