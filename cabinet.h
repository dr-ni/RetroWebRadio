/*
 * RetroWebRadio - optional radio cabinet around the dial
 * GNU GENERAL PUBLIC LICENSE Version 3
 */
#ifndef CABINET_H
#define CABINET_H

#include <SDL.h>

/* logical size of the whole cabinet and position of the 644x428 dial */
#define CAB_W      844
#define CAB_H      706
#define CAB_DIAL_X 100
#define CAB_DIAL_Y 96

/* push buttons below the dial, left to right */
enum {
  CAB_KEY_VOLUP, CAB_KEY_PLAY, CAB_KEY_VOLDOWN,
  CAB_KEY_LEFT, CAB_KEY_UP, CAB_KEY_DOWN, CAB_KEY_RIGHT,
  CAB_KEYS
};

/* cabinet_hit results; a key returns CAB_HIT_KEY + CAB_KEY_... */
enum { CAB_HIT_NONE = -2, CAB_HIT_DIAL = -1, CAB_HIT_KEY = 0 };

/* build the textures once (font_path: fallback for the key labels); 0 on success */
int cabinet_init(SDL_Renderer *r, const char *font_path);

/* case, dial texture, keys and pilot lamp; 'pressed' key (or -1) and
   'latched' keys (bit mask, e.g. play while playing) are drawn pressed down */
void cabinet_render(SDL_Renderer *r, SDL_Texture *dial, int pressed, unsigned latched, int lamp);

/* what is at logical position x,y */
int cabinet_hit(int x, int y);

#endif
