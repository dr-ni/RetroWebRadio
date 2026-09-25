/*
 * RetroWebRadio - optional radio cabinet around the dial
 * GNU GENERAL PUBLIC LICENSE Version 3
 */
#ifndef CABINET_H
#define CABINET_H

#include <SDL.h>

/* logical size of the whole cabinet and position of the 644x428 dial */
#define CAB_W      1030
#define CAB_H      680
#define CAB_DIAL_X 346
#define CAB_DIAL_Y 40

enum {
  CAB_HIT_NONE, CAB_HIT_DIAL,
  CAB_HIT_VOL_DOWN, CAB_HIT_VOL_UP,   /* left / right half of the volume knob */
  CAB_HIT_TUNE_DOWN, CAB_HIT_TUNE_UP  /* left / right half of the tuning knob */
};

/* build the textures once; 0 on success */
int cabinet_init(SDL_Renderer *r);

/* case + dial texture + knob markers; vol/tune in 0..1, < 0 hides the marker */
void cabinet_render(SDL_Renderer *r, SDL_Texture *dial, double vol, double tune);

/* what is at logical position x,y */
int cabinet_hit(int x, int y);

#endif
