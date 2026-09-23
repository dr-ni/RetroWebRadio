#define _DEFAULT_SOURCE  /* M_PI */
/*
 * RetroWebRadio - tuning noise
 *
 * Plays radio static through SDL audio while the dial is between stations
 * (only noise) or slightly off a station (noise over the music from mpd).
 * The static is band-limited white noise with a slow fading and a little
 * crackle; its level follows noise_set() smoothly, so moving the tuner
 * never clicks.
 *
 * GNU GENERAL PUBLIC LICENSE Version 3
 */
#include <stdint.h>
#include <math.h>
#include <SDL.h>
#include "noise.h"

#define RATE 44100

static SDL_AudioDeviceID dev;
static SDL_atomic_t target_milli;   /* requested level * 1000 */
static double gain, lp1, lp2, hp, fade_phase, fade_speed = 0.3;
static uint32_t rng = 0x12345678u;

static double white(void)
{
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return (rng / 4294967295.0) * 2 - 1;
}

static void callback(void *userdata, Uint8 *stream, int len)
{
  int16_t *out = (int16_t *)stream;
  int i, n = len / 2;
  double target = SDL_AtomicGet(&target_milli) / 1000.0;

  (void)userdata;
  for (i = 0; i < n; i++) {
    double v, fade;

    gain += (target - gain) * 0.0008;              /* ~30 ms smoothing */
    if (gain < 1e-5) {
      out[i] = 0;
      continue;
    }
    /* hiss: white noise, low-passed twice (~4 kHz), without rumble */
    v = white();
    lp1 += 0.40 * (v - lp1);
    lp2 += 0.40 * (lp1 - lp2);
    hp += 0.002 * (lp2 - hp);
    v = (lp2 - hp) * 1.8;
    /* slow fading like a weak signal */
    fade_phase += fade_speed * 2 * M_PI / RATE;
    if (fade_phase > 2 * M_PI) {
      fade_phase -= 2 * M_PI;
      fade_speed = 0.15 + 0.5 * (white() + 1) / 2;
    }
    fade = 0.8 + 0.2 * sin(fade_phase);
    /* occasional crackle */
    if ((rng & 0x3fff) == 0)
      v += white() * 2.5;
    v *= gain * fade;
    if (v > 1)
      v = 1;
    if (v < -1)
      v = -1;
    out[i] = (int16_t)(v * 32000);
  }
}

int noise_init(void)
{
  SDL_AudioSpec want, have;

  if (dev != 0)
    return 0;
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    return -1;
  SDL_zero(want);
  want.freq = RATE;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = 1024;
  want.callback = callback;
  dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (dev == 0)
    return -1;
  SDL_AtomicSet(&target_milli, 0);
  SDL_PauseAudioDevice(dev, 0);
  return 0;
}

void noise_set(double level)
{
  if (level < 0)
    level = 0;
  if (level > 1)
    level = 1;
  SDL_AtomicSet(&target_milli, (int)(level * 1000));
}

void noise_close(void)
{
  if (dev != 0) {
    SDL_CloseAudioDevice(dev);
    dev = 0;
  }
}
