#ifndef V4_AUDIO_H_
#define V4_AUDIO_H_

// v4_audio.h — Sound for the TFR/911H.
//
// Output: an 8-bit PWM "DAC" on GPIO 8 (J3 pin 1), carrier ~977 kHz.
// Wire it to a PAM8302 amplifier: J3 pin 2 (VBUS) to Vin, J3 pin 6 (GND)
// to GND and A-, and J3 pin 1 through 1 kOhm to A+, with 10 nF from A+
// to GND to filter out the carrier.
//
// A 31.25 kHz timer interrupt on core 0 mixes:
//   * an SN76489 sound chip (3 square-wave tones + noise, 16 volume steps)
//     written at $FF41, where the CoCo's Game Master Cartridge has one;
//   * keyclicks, the terminal bell (BEL, $07), and Tcl's `beep`.
// $FF42 is a control register: bit 0 enables keyclicks, bit 1 the bell.
//
// The bus loop (core 1) only queues chip writes and sets request flags;
// all synthesis state belongs to the interrupt.

#include <hardware/pwm.h>
#include <hardware/timer.h>
#include <hardware/irq.h>

namespace audio {

constexpr uint PIN = 8;
constexpr uint ALARM = 2;            // the default alarm pool uses 3
constexpr uint SAMPLE_US = 32;       // 31250 Hz
constexpr uint SAMPLE_HZ = 1000000 / SAMPLE_US;
constexpr uint CHIP_HZ = 3579545;    // SN76489 clock
// Chip counter ticks (CHIP_HZ / 16) per sample, in 16.16 fixed point.
constexpr uint32_t STEP =
    (uint32_t)((double)CHIP_HZ / 16.0 / SAMPLE_HZ * 65536.0);

// Amplitude for each attenuation step (2 dB each; 15 is off).  Four
// voices at full volume stay inside the 8-bit range around 128.
const byte Amp[16] = {30, 24, 19, 15, 12, 9, 8, 6, 5, 4, 3, 2, 2, 1, 1, 0};

// ── Shared with core 1 and the rest of core 0 ──
volatile byte control = 3;           // $FF42: bit 0 keyclick, bit 1 bell
volatile byte fifo[256];             // SN76489 writes from the bus loop
volatile byte fifo_head;             // written by core 1
volatile byte fifo_tail;             // written by the interrupt
volatile bool click_request, bell_request, beep_request;
volatile uint beep_hz, beep_ms;
volatile uint32_t stat_chip_writes;

// ── Owned by the interrupt ──
uint16_t tone_n[3] = {1024, 1024, 1024};
byte atten[4] = {15, 15, 15, 15};
byte noise_ctrl;
uint16_t lfsr = 0x4000;
byte latch_ch, latch_vol;
uint32_t count[4];
byte out[4];
// One effect voice for clicks, the bell and beeps.
uint fx_half;                        // samples per half period
uint fx_left;                        // samples left to play
uint fx_phase;
int fx_amp, fx_decay_every, fx_decay_count;
uint32_t next_alarm;

// From core 1, in the middle of a bus cycle: just queue it.
FORCE_INLINE void IN_RAM WriteChip(byte b) {
  byte h = fifo_head;
  if ((byte)(h + 1) != fifo_tail) {
    fifo[h] = b;
    __dmb();
    fifo_head = (byte)(h + 1);
  }
}

FORCE_INLINE void IN_RAM Bell() {
  if (control & 2) bell_request = true;
}

void Click() {
  if (control & 1) click_request = true;
}

void Beep(uint hz, uint ms) {
  beep_hz = hz;
  beep_ms = ms;
  __dmb();
  beep_request = true;
}

void IN_RAM ApplyWrite(byte b) {
  byte data;
  if (b & 0x80) {
    latch_ch = (b >> 5) & 3;
    latch_vol = b & 0x10;
    data = b & 0x0F;
    if (latch_vol) {
      atten[latch_ch] = data;
    } else if (latch_ch == 3) {
      noise_ctrl = data & 7;
      lfsr = 0x4000;
    } else {
      uint n = (tone_n[latch_ch] & 0x3F0) | data;
      tone_n[latch_ch] = n ? n : 1024;
    }
  } else {
    if (latch_vol) {
      atten[latch_ch] = b & 0x0F;
    } else if (latch_ch == 3) {
      noise_ctrl = b & 7;
      lfsr = 0x4000;
    } else {
      uint n = (tone_n[latch_ch] & 0x00F) | ((b & 0x3F) << 4);
      tone_n[latch_ch] = n ? n : 1024;
    }
  }
}

void IN_RAM StartFx(uint hz, uint ms, int amp, int decay_every) {
  fx_half = SAMPLE_HZ / (2 * (hz ? hz : 1));
  if (fx_half == 0) fx_half = 1;
  fx_left = SAMPLE_HZ / 1000 * ms;
  fx_phase = 0;
  fx_amp = amp;
  fx_decay_every = decay_every;
  fx_decay_count = 0;
}

FORCE_INLINE int IN_RAM Voice(uint i, uint period_ticks) {
  // A tone that would toggle more than once per sample is far above
  // hearing; the real chip then just sits high (used for sample playback).
  if (period_ticks < 8) {
    out[i] = 1;
  } else {
    count[i] += STEP;
    uint32_t p = period_ticks << 16;
    if (count[i] >= p) {
      count[i] -= p;
      out[i] ^= 1;
      if (i == 3 && out[i]) {  // noise: shift on each rising edge
        uint fb = (noise_ctrl & 4) ? ((lfsr ^ (lfsr >> 1)) & 1) : (lfsr & 1);
        lfsr = (uint16_t)((lfsr >> 1) | (fb << 14));
      }
    }
  }
  int a = Amp[atten[i]];
  if (i == 3) return (lfsr & 1) ? a : -a;
  return out[i] ? a : -a;
}

void IN_RAM Interrupt() {
  timer_hw->intr = 1u << ALARM;
  next_alarm += SAMPLE_US;
  uint32_t now = timer_hw->timerawl;
  if ((int32_t)(next_alarm - now) <= 1) next_alarm = now + SAMPLE_US;
  timer_hw->alarm[ALARM] = next_alarm;

  while (fifo_tail != fifo_head) {
    ApplyWrite(fifo[fifo_tail]);
    stat_chip_writes++;
    fifo_tail = (byte)(fifo_tail + 1);
  }
  if (beep_request) {
    beep_request = false;
    StartFx(beep_hz, beep_ms, 28, 0);
  } else if (bell_request) {
    bell_request = false;
    StartFx(880, 150, 28, 0);
  } else if (click_request) {
    click_request = false;
    StartFx(3900, 2, 40, 2);  // a short, decaying tick
  }

  int s = 128;
  s += Voice(0, tone_n[0]);
  s += Voice(1, tone_n[1]);
  s += Voice(2, tone_n[2]);
  uint nr = noise_ctrl & 3;
  s += Voice(3, nr == 3 ? tone_n[2] : (16u << nr));
  if (fx_left) {
    fx_left--;
    if (++fx_phase >= 2 * fx_half) fx_phase = 0;
    s += (fx_phase < fx_half) ? fx_amp : -fx_amp;
    if (fx_decay_every && ++fx_decay_count >= fx_decay_every) {
      fx_decay_count = 0;
      if (fx_amp > 0) fx_amp--;
    }
  }
  if (s < 0) s = 0;
  if (s > 255) s = 255;
  pwm_set_gpio_level(PIN, (uint16_t)s);
}

// Core 0, once, after the GPIOs are set up.
void Init() {
  gpio_set_function(PIN, GPIO_FUNC_PWM);
  uint slice = pwm_gpio_to_slice_num(PIN);
  pwm_config cfg = pwm_get_default_config();
  pwm_config_set_wrap(&cfg, 255);
  pwm_config_set_clkdiv(&cfg, 1.0f);
  pwm_init(slice, &cfg, true);
  for (uint level = 0; level <= 128; level++) {  // ease up to mid-scale: no pop
    pwm_set_gpio_level(PIN, level);
    sleep_us(400);
  }

  hardware_alarm_claim(ALARM);
  uint irq = hardware_alarm_get_irq_num(ALARM);
  irq_set_exclusive_handler(irq, Interrupt);
  hw_set_bits(&timer_hw->inte, 1u << ALARM);
  irq_set_enabled(irq, true);
  next_alarm = timer_hw->timerawl + SAMPLE_US;
  timer_hw->alarm[ALARM] = next_alarm;
}

}  // namespace audio

// Tcl:  beep ?hz? ?ms?
int beep_cmd(ClientData, Tcl_Interp* interp, int argc, char* argv[]) {
  uint hz = argc > 1 ? (uint)atoi(argv[1]) : 880;
  uint ms = argc > 2 ? (uint)atoi(argv[2]) : 200;
  audio::Beep(hz, ms);
  return TCL_OK;
}

// Tcl:  click on|off   (keyclicks in NitrOS-9)
int click_cmd(ClientData, Tcl_Interp* interp, int argc, char* argv[]) {
  if (argc > 1) {
    if (!strcmp(argv[1], "off")) audio::control &= ~1;
    else audio::control |= 1;
  }
  Tcl_SetResult(interp, (char*)((audio::control & 1) ? "on" : "off"), TCL_STATIC);
  return TCL_OK;
}

#endif  // V4_AUDIO_H_
