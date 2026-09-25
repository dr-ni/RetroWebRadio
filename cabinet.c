#define _DEFAULT_SOURCE  /* M_PI */
/*
 * RetroWebRadio - optional radio cabinet around the dial
 *
 * A tube radio case in the style of the icon: wooden body, the dial
 * behind a cream bezel and a row of ivory piano-key buttons below it:
 *
 *   [Vol +] [Play/Pause] [Vol -]   [>>] [^] [v] [<<]
 *
 * The case and the keys (normal and pressed) are rendered once with
 * anti-aliased shapes into textures; per frame only textures are copied.
 *
 * GNU GENERAL PUBLIC LICENSE Version 3
 */
#include <math.h>
#include <stdint.h>
#include "cabinet.h"

#define BODY_COL   0xc8782a
#define EDGE_COL   0x4a2a0e
#define DARK_COL   0x3b2410
#define CREAM_COL  0xf3e3a8
#define IVORY_COL  0xefe4c8
#define IVORY_DN   0xd9ccab
#define SYMBOL_COL 0x3b2410
#define OUTSIDE    0x141414

#define BODY_BOTTOM (CAB_H - 26)
#define SLOT_X0    (CAB_DIAL_X - 12)
#define SLOT_X1    (CAB_DIAL_X + 644 + 12)
#define SLOT_Y0    (CAB_DIAL_Y + 428 + 34)
#define SLOT_Y1    (SLOT_Y0 + 86)
#define KEY_W      80
#define KEY_H      64
#define KEY_GAP    6
#define GROUP_GAP  40   /* between the volume and the tuning group */
#define PRESS_DY   4    /* a pressed key sinks by this much */

static SDL_Texture *cab_tex, *key_tex[CAB_KEYS][2];

/* ---------------------------------------------------------------- */
/* anti-aliased drawing on an ARGB8888 surface                      */
/* ---------------------------------------------------------------- */

static void blend(SDL_Surface *s, int x, int y, uint32_t rgb, double a)
{
  uint32_t *p, d;
  double da, oa;
  int r, g, b;

  if (a <= 0 || x < 0 || y < 0 || x >= s->w || y >= s->h)
    return;
  if (a > 1)
    a = 1;
  p = (uint32_t *)((uint8_t *)s->pixels + y * s->pitch) + x;
  d = *p;
  da = ((d >> 24) & 0xff) / 255.0;
  oa = a + da * (1 - a);   /* "over" onto a possibly transparent pixel */
  if (oa <= 0)
    return;
  r = (int)((((d >> 16) & 0xff) * da * (1 - a) + ((rgb >> 16) & 0xff) * a) / oa);
  g = (int)((((d >> 8) & 0xff) * da * (1 - a) + ((rgb >> 8) & 0xff) * a) / oa);
  b = (int)(((d & 0xff) * da * (1 - a) + (rgb & 0xff) * a) / oa);
  *p = ((uint32_t)(oa * 255 + 0.5) << 24) | (uint32_t)(r << 16) | (uint32_t)(g << 8) | (uint32_t)b;
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

/* filled triangle (clockwise or not), anti-aliased */
static void tri(SDL_Surface *s, double ax, double ay, double bx, double by,
                double cx, double cy, uint32_t col)
{
  double px[3] = { ax, bx, cx }, py[3] = { ay, by, cy };
  double area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
  double sign = area > 0 ? 1 : -1;
  int x, y, i;
  int x0 = (int)fmin(ax, fmin(bx, cx)) - 1, x1 = (int)fmax(ax, fmax(bx, cx)) + 1;
  int y0 = (int)fmin(ay, fmin(by, cy)) - 1, y1 = (int)fmax(ay, fmax(by, cy)) + 1;

  for (y = y0; y <= y1; y++)
    for (x = x0; x <= x1; x++) {
      double d = -1e9;
      for (i = 0; i < 3; i++) {  /* max of signed distances to the edges */
        double ex = px[(i + 1) % 3] - px[i], ey = py[(i + 1) % 3] - py[i];
        double len = hypot(ex, ey);
        double e = -sign * ((x + 0.5 - px[i]) * ey - (y + 0.5 - py[i]) * ex) / len;
        if (-e > d)
          d = -e;
      }
      blend(s, x, y, col, 0.5 - d);
    }
}

/* ---------------------------------------------------------------- */

static void key_rect(int k, int *x, int *y)
{
  int group = k >= CAB_KEY_RIGHT;
  int total = CAB_KEYS * KEY_W + (CAB_KEYS - 2) * KEY_GAP + GROUP_GAP;
  *x = (SLOT_X0 + SLOT_X1 - total) / 2 + k * (KEY_W + KEY_GAP) + group * (GROUP_GAP - KEY_GAP);
  *y = SLOT_Y0 + (SLOT_Y1 - SLOT_Y0 - KEY_H) / 2 - 2;
}

static SDL_Surface *draw_case(void)
{
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, CAB_W, CAB_H, 32, SDL_PIXELFORMAT_ARGB8888);
  int x, y;

  if (s == NULL)
    return NULL;
  SDL_FillRect(s, NULL, 0xff000000u | OUTSIDE);

  /* feet */
  rrect(s, 70, BODY_BOTTOM - 20, 160, CAB_H - 4, 8, DARK_COL, 0, 0);
  rrect(s, CAB_W - 160, BODY_BOTTOM - 20, CAB_W - 70, CAB_H - 4, 8, DARK_COL, 0, 0);

  /* wooden body with a subtle grain */
  rrect(s, 4, 4, CAB_W - 4, BODY_BOTTOM, 40, BODY_COL, 6, EDGE_COL);
  for (y = 12; y < BODY_BOTTOM - 8; y++)
    for (x = 12; x < CAB_W - 12; x++) {
      double inside = -sd_rrect(x + 0.5, y + 0.5, 4, 4, CAB_W - 4, BODY_BOTTOM, 40) - 7;
      double grain = sin(y * 0.21 + sin(x * 0.013) * 2.5 + sin(x * 0.051) * 0.6);
      if (inside > 0)
        blend(s, x, y, grain > 0 ? 0xa05a18 : 0xe09040,
              (inside > 2 ? 1 : inside / 2) * 0.10 * fabs(grain));
    }

  /* bezel around the dial */
  rrect(s, CAB_DIAL_X - 12, CAB_DIAL_Y - 12, CAB_DIAL_X + 644 + 12, CAB_DIAL_Y + 428 + 12,
        10, CREAM_COL, 3, EDGE_COL);

  /* recessed slot for the keys */
  rrect(s, SLOT_X0, SLOT_Y0, SLOT_X1, SLOT_Y1, 10, 0x1e1208, 3, EDGE_COL);
  return s;
}

/* symbol of key k, centred at cx,cy */
static void draw_symbol(SDL_Surface *s, int k, double cx, double cy)
{
  double t = 9;  /* half size of arrows */

  switch (k) {
  case CAB_KEY_VOLUP:
  case CAB_KEY_VOLDOWN:
    /* speaker + plus/minus */
    rrect(s, cx - 17, cy - 5, cx - 11, cy + 5, 1, SYMBOL_COL, 0, 0);
    tri(s, cx - 12, cy - 5, cx - 3, cy - 12, cx - 3, cy + 12, SYMBOL_COL);
    tri(s, cx - 12, cy + 5, cx - 3, cy + 12, cx - 12, cy - 5, SYMBOL_COL);
    rrect(s, cx + 3, cy - 1.8, cx + 17, cy + 1.8, 1, SYMBOL_COL, 0, 0);
    if (k == CAB_KEY_VOLUP)
      rrect(s, cx + 8.2, cy - 7, cx + 11.8, cy + 7, 1, SYMBOL_COL, 0, 0);
    break;
  case CAB_KEY_PLAY:
    tri(s, cx - 15, cy - 10, cx - 15, cy + 10, cx - 1, cy, SYMBOL_COL);
    rrect(s, cx + 4, cy - 10, cx + 8, cy + 10, 1, SYMBOL_COL, 0, 0);
    rrect(s, cx + 12, cy - 10, cx + 16, cy + 10, 1, SYMBOL_COL, 0, 0);
    break;
  case CAB_KEY_RIGHT:  /* scan right: >> */
    tri(s, cx - 12, cy - t, cx - 12, cy + t, cx, cy, SYMBOL_COL);
    tri(s, cx, cy - t, cx, cy + t, cx + 12, cy, SYMBOL_COL);
    break;
  case CAB_KEY_LEFT:   /* scan left: << */
    tri(s, cx + 12, cy - t, cx + 12, cy + t, cx, cy, SYMBOL_COL);
    tri(s, cx, cy - t, cx, cy + t, cx - 12, cy, SYMBOL_COL);
    break;
  case CAB_KEY_UP:     /* next page */
    tri(s, cx - 11, cy + 6, cx + 11, cy + 6, cx, cy - 8, SYMBOL_COL);
    break;
  case CAB_KEY_DOWN:   /* previous page */
    tri(s, cx - 11, cy - 6, cx + 11, cy - 6, cx, cy + 8, SYMBOL_COL);
    break;
  }
}

/* one key, 'down' = pressed look; surface is transparent around it */
static SDL_Surface *draw_key(int k, int down)
{
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, KEY_W, KEY_H + PRESS_DY, 32,
                                                  SDL_PIXELFORMAT_ARGB8888);
  double dy = down ? PRESS_DY : 0;

  if (s == NULL)
    return NULL;
  SDL_FillRect(s, NULL, 0);
  /* side / shadow, then the key face with a light top edge */
  rrect(s, 0, dy + 4, KEY_W, KEY_H + dy, 7, down ? 0x8e8065 : 0xa89878, 0, 0);
  rrect(s, 0, dy, KEY_W, KEY_H - 6 + dy + (down ? 3 : 0), 7,
        down ? IVORY_DN : IVORY_COL, 1.5, 0x7a6a4e);
  rrect(s, 4, dy + 2, KEY_W - 4, dy + 7, 3, down ? 0xe4d8ba : 0xfbf5e6, 0, 0);
  draw_symbol(s, k, KEY_W / 2.0, (KEY_H - 6) / 2.0 + dy + 1);
  return s;
}

int cabinet_init(SDL_Renderer *r)
{
  SDL_Surface *s;
  int k, d;

  if (cab_tex != NULL)
    return 0;
  if ((s = draw_case()) == NULL)
    return -1;
  cab_tex = SDL_CreateTextureFromSurface(r, s);
  SDL_FreeSurface(s);

  for (k = 0; k < CAB_KEYS; k++)
    for (d = 0; d < 2; d++) {
      SDL_Surface *ks = draw_key(k, d);
      if (ks == NULL)
        return -1;
      key_tex[k][d] = SDL_CreateTextureFromSurface(r, ks);
      SDL_SetTextureBlendMode(key_tex[k][d], SDL_BLENDMODE_BLEND);
      SDL_FreeSurface(ks);
    }
  return cab_tex != NULL ? 0 : -1;
}

void cabinet_render(SDL_Renderer *r, SDL_Texture *dial, int pressed, unsigned latched)
{
  SDL_Rect dst = { CAB_DIAL_X, CAB_DIAL_Y, 644, 428 };
  int k;

  SDL_RenderCopy(r, cab_tex, NULL, NULL);
  SDL_RenderCopy(r, dial, NULL, &dst);
  for (k = 0; k < CAB_KEYS; k++) {
    SDL_Rect kr;
    int down = (k == pressed) || (latched & (1u << k));
    key_rect(k, &kr.x, &kr.y);
    kr.w = KEY_W;
    kr.h = KEY_H + PRESS_DY;
    SDL_RenderCopy(r, key_tex[k][down], NULL, &kr);
  }
}

int cabinet_hit(int x, int y)
{
  int k;

  if (x >= CAB_DIAL_X && x < CAB_DIAL_X + 644 && y >= CAB_DIAL_Y && y < CAB_DIAL_Y + 428)
    return CAB_HIT_DIAL;
  for (k = 0; k < CAB_KEYS; k++) {
    int kx, ky;
    key_rect(k, &kx, &ky);
    if (x >= kx && x < kx + KEY_W && y >= ky && y < ky + KEY_H + PRESS_DY)
      return CAB_HIT_KEY + k;
  }
  return CAB_HIT_NONE;
}
