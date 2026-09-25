#define _DEFAULT_SOURCE  /* M_PI */
/*
 * RetroWebRadio - optional radio cabinet around the dial
 *
 * Draws a tube radio case in the style of the tray/window icon: wooden
 * body, speaker grille on the left, the dial behind a cream bezel and
 * two knobs (volume, tuning) below it. The case is rendered once into a
 * texture with anti-aliased shapes (signed distance per pixel); only the
 * knob markers move.
 *
 * GNU GENERAL PUBLIC LICENSE Version 3
 */
#include <math.h>
#include <stdint.h>
#include "cabinet.h"

#define BODY_COL   0xc8782a
#define EDGE_COL   0x4a2a0e
#define DARK_COL   0x3b2410
#define BAR_COL    0xe8c98a
#define CREAM_COL  0xf3e3a8
#define OUTSIDE    0x141414

#define GRILLE_X0  36
#define GRILLE_X1  306
#define BODY_BOTTOM (CAB_H - 30)
#define KNOB_R     50
#define KNOB_DOT_R 34   /* radius of the marker path */

static SDL_Texture *cab_tex, *dot_tex;

/* ---------------------------------------------------------------- */
/* anti-aliased drawing on an ARGB8888 surface                      */
/* ---------------------------------------------------------------- */

static void blend(SDL_Surface *s, int x, int y, uint32_t rgb, double a)
{
  uint32_t *p, d;
  int r, g, b;

  if (a <= 0 || x < 0 || y < 0 || x >= s->w || y >= s->h)
    return;
  if (a > 1)
    a = 1;
  p = (uint32_t *)((uint8_t *)s->pixels + y * s->pitch) + x;
  d = *p;
  r = (int)(((d >> 16) & 0xff) * (1 - a) + ((rgb >> 16) & 0xff) * a);
  g = (int)(((d >> 8) & 0xff) * (1 - a) + ((rgb >> 8) & 0xff) * a);
  b = (int)((d & 0xff) * (1 - a) + (rgb & 0xff) * a);
  *p = 0xff000000u | (uint32_t)(r << 16) | (uint32_t)(g << 8) | (uint32_t)b;
}

/* signed distance to a rounded rectangle (negative inside) */
static double sd_rrect(double px, double py, double x0, double y0, double x1, double y1, double r)
{
  double cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
  double qx = fabs(px - cx) - ((x1 - x0) / 2 - r);
  double qy = fabs(py - cy) - ((y1 - y0) / 2 - r);
  double ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
  double in = qx > qy ? qx : qy;
  return sqrt(ox * ox + oy * oy) + (in < 0 ? in : 0) - r;
}

/* filled rounded rect, optional border of width bw in colour bcol */
static void rrect(SDL_Surface *s, double x0, double y0, double x1, double y1, double r,
                  uint32_t col, double bw, uint32_t bcol)
{
  int x, y;
  for (y = (int)y0 - 1; y <= (int)y1 + 1; y++)
    for (x = (int)x0 - 1; x <= (int)x1 + 1; x++) {
      double d = sd_rrect(x + 0.5, y + 0.5, x0, y0, x1, y1, r);
      blend(s, x, y, col, 0.5 - d);
      if (bw > 0)
        blend(s, x, y, bcol, 0.5 - (fabs(d + bw / 2) - bw / 2));
    }
}

static void circle(SDL_Surface *s, double cx, double cy, double r,
                   uint32_t col, double bw, uint32_t bcol)
{
  rrect(s, cx - r, cy - r, cx + r, cy + r, r, col, bw, bcol);
}

/* ---------------------------------------------------------------- */

static void knob_center(int which, int *x, int *y)
{
  *x = CAB_DIAL_X + (which == 0 ? 644 * 3 / 10 : 644 * 7 / 10);
  *y = CAB_DIAL_Y + 428 + (BODY_BOTTOM - CAB_DIAL_Y - 428) / 2 - 4;
}

static SDL_Surface *draw_case(void)
{
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, CAB_W, CAB_H, 32, SDL_PIXELFORMAT_ARGB8888);
  int x, y, k;

  if (s == NULL)
    return NULL;
  SDL_FillRect(s, NULL, 0xff000000u | OUTSIDE);

  /* feet */
  rrect(s, 110, BODY_BOTTOM - 20, 200, CAB_H - 4, 8, DARK_COL, 0, 0);
  rrect(s, CAB_W - 200, BODY_BOTTOM - 20, CAB_W - 110, CAB_H - 4, 8, DARK_COL, 0, 0);

  /* wooden body with a subtle grain */
  rrect(s, 4, 4, CAB_W - 4, BODY_BOTTOM, 42, BODY_COL, 6, EDGE_COL);
  for (y = 12; y < BODY_BOTTOM - 8; y++)
    for (x = 12; x < CAB_W - 12; x++) {
      double inside = -sd_rrect(x + 0.5, y + 0.5, 4, 4, CAB_W - 4, BODY_BOTTOM, 42) - 7;
      double grain = sin(y * 0.21 + sin(x * 0.013) * 2.5 + sin(x * 0.051) * 0.6);
      if (inside > 0)
        blend(s, x, y, grain > 0 ? 0xa05a18 : 0xe09040,
              (inside > 2 ? 1 : inside / 2) * 0.10 * fabs(grain));
    }

  /* speaker grille */
  rrect(s, GRILLE_X0, 40, GRILLE_X1, BODY_BOTTOM - 36, 20, DARK_COL, 3, EDGE_COL);
  for (k = 0; k < 10; k++) {  /* 10 bars of 10 px, pitch 24, centred */
    x = GRILLE_X0 + (GRILLE_X1 - GRILLE_X0 - (9 * 24 + 10)) / 2 + k * 24;
    rrect(s, x, 64, x + 10, BODY_BOTTOM - 60, 5, BAR_COL, 0, 0);
  }

  /* bezel around the dial */
  rrect(s, CAB_DIAL_X - 12, CAB_DIAL_Y - 12, CAB_DIAL_X + 644 + 12, CAB_DIAL_Y + 428 + 12,
        10, CREAM_COL, 3, EDGE_COL);

  /* knobs with a tick scale */
  for (k = 0; k < 2; k++) {
    int cx, cy, t;
    knob_center(k, &cx, &cy);
    for (t = 0; t <= 10; t++) {
      double a = (-135 + 27 * t) * M_PI / 180;
      circle(s, cx + sin(a) * (KNOB_R + 12), cy - cos(a) * (KNOB_R + 12), 2.2, DARK_COL, 0, 0);
    }
    circle(s, cx, cy + 3, KNOB_R, 0x20140a, 0, 0);          /* shadow */
    circle(s, cx, cy, KNOB_R, DARK_COL, 5, CREAM_COL);
    circle(s, cx, cy, KNOB_R - 16, 0x4a2e14, 0, 0);
  }
  return s;
}

int cabinet_init(SDL_Renderer *r)
{
  SDL_Surface *s, *d;

  if (cab_tex != NULL)
    return 0;
  if ((s = draw_case()) == NULL)
    return -1;
  cab_tex = SDL_CreateTextureFromSurface(r, s);
  SDL_FreeSurface(s);

  /* marker dot, transparent around it */
  d = SDL_CreateRGBSurfaceWithFormat(0, 20, 20, 32, SDL_PIXELFORMAT_ARGB8888);
  if (d != NULL) {
    int x, y;
    for (y = 0; y < 20; y++)
      for (x = 0; x < 20; x++) {
        double dist = hypot(x + 0.5 - 10, y + 0.5 - 10) - 7;
        double a = 0.5 - dist;
        a = a < 0 ? 0 : a > 1 ? 1 : a;
        ((uint32_t *)((uint8_t *)d->pixels + y * d->pitch))[x] =
            ((uint32_t)(a * 255) << 24) | CREAM_COL;
      }
    dot_tex = SDL_CreateTextureFromSurface(r, d);
    SDL_SetTextureBlendMode(dot_tex, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(d);
  }
  return cab_tex != NULL ? 0 : -1;
}

void cabinet_render(SDL_Renderer *r, SDL_Texture *dial, double vol, double tune)
{
  SDL_Rect dst = { CAB_DIAL_X, CAB_DIAL_Y, 644, 428 };
  double v[2];
  int k;

  v[0] = vol;
  v[1] = tune;
  SDL_RenderCopy(r, cab_tex, NULL, NULL);
  SDL_RenderCopy(r, dial, NULL, &dst);
  for (k = 0; k < 2 && dot_tex != NULL; k++) {
    int cx, cy;
    double a;
    SDL_Rect dr;

    if (v[k] < 0)
      continue;  /* unknown (e.g. no volume control) */
    a = (-135 + 270 * (v[k] > 1 ? 1 : v[k])) * M_PI / 180;
    knob_center(k, &cx, &cy);
    dr.x = (int)lround(cx + sin(a) * KNOB_DOT_R) - 10;
    dr.y = (int)lround(cy - cos(a) * KNOB_DOT_R) - 10;
    dr.w = dr.h = 20;
    SDL_RenderCopy(r, dot_tex, NULL, &dr);
  }
}

int cabinet_hit(int x, int y)
{
  int k;

  if (x >= CAB_DIAL_X && x < CAB_DIAL_X + 644 && y >= CAB_DIAL_Y && y < CAB_DIAL_Y + 428)
    return CAB_HIT_DIAL;
  for (k = 0; k < 2; k++) {
    int cx, cy;
    knob_center(k, &cx, &cy);
    if (hypot(x - cx, y - cy) <= KNOB_R + 14)
      return k == 0 ? (x < cx ? CAB_HIT_VOL_DOWN : CAB_HIT_VOL_UP)
                    : (x < cx ? CAB_HIT_TUNE_DOWN : CAB_HIT_TUNE_UP);
  }
  return CAB_HIT_NONE;
}
