/*
 * RetroWebRadio - optional radio cabinet around the dial
 * GNU GENERAL PUBLIC LICENSE Version 3
 */
#ifndef CABINET_H
#define CABINET_H

#include <SDL.h>

/* logical size of the whole cabinet and position of the 644x428 dial */
#define CAB_W      724
#define CAB_H      716
#define CAB_DIAL_X 40
#define CAB_DIAL_Y 110

/* piano keys below the dial, left to right */
enum {
  CAB_KEY_VOLUP, CAB_KEY_PLAY, CAB_KEY_VOLDOWN,
  CAB_KEY_LEFT, CAB_KEY_UP, CAB_KEY_DOWN, CAB_KEY_RIGHT,
  CAB_KEYS
};

/* cabinet_hit results; a key returns CAB_HIT_KEY + CAB_KEY_... */
enum { CAB_HIT_NONE = -2, CAB_HIT_DIAL = -1, CAB_HIT_KEY = 0 };

/* build the textures once; 0 on success */
int cabinet_init(SDL_Renderer *r);

/* case, dial texture and keys; 'pressed' key (or -1) and 'latched' keys
   (bit mask, e.g. play/pause while playing) are drawn pressed down */
void cabinet_render(SDL_Renderer *r, SDL_Texture *dial, int pressed, unsigned latched);

/* what is at logical position x,y */
int cabinet_hit(int x, int y);

#endif
