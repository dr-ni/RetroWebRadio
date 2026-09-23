/*
 * RetroWebRadio - tuning noise
 * GNU GENERAL PUBLIC LICENSE Version 3
 */
#ifndef NOISE_H
#define NOISE_H

/* open the default audio output; 0 on success */
int noise_init(void);
/* static level 0..1 (smoothed internally) */
void noise_set(double level);
void noise_close(void);

#endif
