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
  CAB_KEY_PLAY, CAB_KEY_VOLUP, CAB_KEY_VOLDOWN,
  CAB_KEY_LEFT, CAB_KEY_UP, CAB_KEY_DOWN, CAB_KEY_RIGHT,
  CAB_KEYS
};

/* cabinet_hit results; a key returns CAB_HIT_KEY + CAB_KEY_... */
enum {
  CAB_HIT_KNOB_RIGHT = -4, CAB_HIT_KNOB_LEFT = -3,   /* step buttons */
  CAB_HIT_NONE = -2, CAB_HIT_DIAL = -1, CAB_HIT_KEY = 0
};

/* build the textures once; font_path: fallback for the key labels,
   tex_path: veneer photos for frame, front panel, lower panel (each may be
   NULL or missing, then a procedural veneer is drawn); 0 on success */
int cabinet_init(SDL_Renderer *r, const char *font_path, const char *const tex_path[3]);

/* case, dial texture, keys, knobs, pilot lamp and magic eye.
   pressed: key index or CAB_HIT_KNOB_* held down (else -1); latched: keys
   drawn pressed (bit mask, e.g. play while playing); lamp: 0 off, 1 lit,
   2 red; eye_open: 0 tuned in .. 1 detuned */
void cabinet_render(SDL_Renderer *r, SDL_Texture *dial, int pressed, unsigned latched,
                    int lamp, double eye_open);

/* window shape of the case (alpha 255 inside), CAB_W x CAB_H; caller frees */
SDL_Surface *cabinet_mask(void);

/* what is at logical position x,y */
int cabinet_hit(int x, int y);

#endif
