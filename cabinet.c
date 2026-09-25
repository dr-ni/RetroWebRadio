#define _DEFAULT_SOURCE  /* M_PI */
/*
 * RetroWebRadio - optional radio cabinet around the dial
 *
 * A 1930s style tube radio: walnut "tombstone" case with an arched top
 * and an Art Deco sunburst, brass trim, the dial behind a brass bezel and
 * a row of Bakelite push buttons below it:
 *
 *   [Vol +] [Play/Pause] [Vol -]   [<<] [^] [v] [>>]
 *
 * The case and the keys (normal and pressed) are rendered once with
 * anti-aliased shapes into textures; per frame only textures are copied.
 *
 * GNU GENERAL PUBLIC LICENSE Version 3
 */
#include <math.h>
#include <stdint.h>
#include "cabinet.h"

#define EDGE_COL   0x24100a
#define BRASS_COL  0xc9a34a
#define BRASS_DARK 0x7a5a1e
#define OUTSIDE    0x141414

#define ARCH_H     70         /* height of the arched top above the body */
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

/* ---- value noise for wood and Bakelite ---------------------------- */

static double hash2(int x, int y)
{
  uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return ((h ^ (h >> 16)) & 0xffffff) / 16777215.0;
}

static double vnoise(double x, double y)
{
  int ix = (int)floor(x), iy = (int)floor(y);
  double fx = x - ix, fy = y - iy;
  double ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
  double a = hash2(ix, iy), b = hash2(ix + 1, iy);
  double c = hash2(ix, iy + 1), d = hash2(ix + 1, iy + 1);
  return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
}

static double fbm(double x, double y)
{
  double v = 0, amp = 0.5;
  int i;
  for (i = 0; i < 5; i++, x *= 2.03, y *= 2.03, amp *= 0.5)
    v += amp * vnoise(x, y);
  return v;  /* ~0..1 */
}

static uint32_t rgbf(double r, double g, double b)
{
  r = r < 0 ? 0 : r > 255 ? 255 : r;
  g = g < 0 ? 0 : g > 255 ? 255 : g;
  b = b < 0 ? 0 : b > 255 ? 255 : b;
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/*
 * Book-matched walnut veneer: mirrored at the centre line like on real
 * cabinets, wavy vertical grain lines with a slow figure, fine pores
 * and a soft varnish sheen.
 */
static uint32_t walnut(double px, double py)
{
  double mx = fabs(px - CAB_W / 2.0), t, lines, fig, pores, sheen, k;
  double warp = fbm(mx * 0.004 + 7.3, py * 0.003) - 0.5;

  fig = fbm(mx * 0.010 + warp * 2.0, py * 0.0022 + 3.1);           /* 0..1 */
  lines = sin(mx * 0.33 + warp * 22 + fbm(mx * 0.03, py * 0.006) * 9);
  lines = pow(0.5 + 0.5 * lines, 6);                                 /* thin dark lines */
  pores = vnoise(px * 0.9, py * 0.08);
  t = 0.35 + 0.65 * fig;
  k = 1.0 - 0.28 * lines - 0.07 * pores;
  /* varnish: a broad soft highlight band */
  sheen = 0.10 * exp(-pow((px - CAB_W * 0.32) / (CAB_W * 0.22), 2));
  return rgbf((52 + 60 * t) * k * (1 + sheen), (24 + 30 * t) * k * (1 + sheen),
              (10 + 14 * t) * k * (1 + sheen));
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
  int group = k >= CAB_KEY_LEFT;  /* second group: tuning */
  int total = CAB_KEYS * KEY_W + (CAB_KEYS - 2) * KEY_GAP + GROUP_GAP;
  *x = (SLOT_X0 + SLOT_X1 - total) / 2 + k * (KEY_W + KEY_GAP) + group * (GROUP_GAP - KEY_GAP);
  *y = SLOT_Y0 + (SLOT_Y1 - SLOT_Y0 - KEY_H) / 2 - 2;
}

/* brushed brass: light at the top edge, darker below, fine streaks */
static uint32_t brass(double py, double y0, double y1, double px)
{
  double v = (py - y0) / (y1 - y0), n = vnoise(px * 0.03, py * 1.7) - 0.5, t;
  v = v < 0 ? 0 : v > 1 ? 1 : v;
  t = 1.25 - 0.55 * v + 0.10 * n;
  if (v < 0.12)
    t += 0.35 * (1 - v / 0.12);   /* bright bevel */
  return rgbf(170 * t, 132 * t, 58 * t);
}

/* rounded rect filled with brushed brass */
static void brass_rrect(SDL_Surface *s, double x0, double y0, double x1, double y1, double r)
{
  int x, y;
  for (y = (int)y0 - 1; y <= (int)y1 + 1; y++)
    for (x = (int)x0 - 1; x <= (int)x1 + 1; x++) {
      double d = sd_rrect(x + 0.5, y + 0.5, x0, y0, x1, y1, r);
      if (d < 1) {
        blend(s, x, y, brass(y + 0.5, y0, y1, x + 0.5), 0.5 - d);
        blend(s, x, y, 0x5a3e12, (0.5 - (fabs(d + 0.75) - 0.75)) * 0.8);  /* dark edge */
      }
    }
}

/* distance to the case outline: straight body with an arched top */
static double sd_case(double px, double py)
{
  double w = CAB_W - 8, cx = CAB_W / 2.0, top = 4, base = top + ARCH_H;
  double R = (w * w / 4 + ARCH_H * ARCH_H) / (2.0 * ARCH_H);   /* arch radius */
  double body = sd_rrect(px, py, 4, base - 30, CAB_W - 4, BODY_BOTTOM, 30);
  double arch = hypot(px - cx, py - (top + R)) - R;
  double cut = py - (base + 10);           /* keep the arch above the body */
  double a = arch > cut ? arch : cut;
  return body < a ? body : a;
}

static SDL_Surface *draw_case(void)
{
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, CAB_W, CAB_H, 32, SDL_PIXELFORMAT_ARGB8888);
  int x, y, i;
  double cx = CAB_W / 2.0;

  if (s == NULL)
    return NULL;
  SDL_FillRect(s, NULL, 0xff000000u | OUTSIDE);

  /* brass ball feet */
  brass_rrect(s, 60, BODY_BOTTOM - 20, 150, CAB_H - 4, 10);
  brass_rrect(s, CAB_W - 150, BODY_BOTTOM - 20, CAB_W - 60, CAB_H - 4, 10);

  /* walnut veneer, darker towards the rounded edge (bevel) */
  for (y = 0; y < CAB_H; y++)
    for (x = 0; x < CAB_W; x++) {
      double d = sd_case(x + 0.5, y + 0.5);
      double cov, shade;
      if (d > 1)
        continue;
      cov = 0.5 - d;
      blend(s, x, y, walnut(x + 0.5, y + 0.5), cov);
      shade = d > -14 ? (d + 14) / 14 : 0;                 /* 0 inside .. 1 at edge */
      blend(s, x, y, EDGE_COL, cov * 0.75 * shade * shade);
    }

  /* brass pinstripe following the outline */
  for (y = 0; y < CAB_H; y++)
    for (x = 0; x < CAB_W; x++) {
      double d = sd_case(x + 0.5, y + 0.5) + 16;   /* 16 px inside */
      if (fabs(d) < 2)
        blend(s, x, y, BRASS_COL, 0.5 - (fabs(d) - 1));
    }

  /* Art Deco sunburst in the arch */
  for (i = -6; i <= 6; i++) {
    double a = i * 12 * M_PI / 180, len = 82 - abs(i) * 6;
    double ox = cx, oy = CAB_DIAL_Y - 14;
    tri(s, ox - 4, oy, ox + 4, oy, ox + sin(a) * len, oy - cos(a) * len,
        i % 2 ? BRASS_DARK : BRASS_COL);
  }
  brass_rrect(s, cx - 14, CAB_DIAL_Y - 28, cx + 14, CAB_DIAL_Y, 14);

  /* brass bezel around the dial, rounded top */
  brass_rrect(s, CAB_DIAL_X - 14, CAB_DIAL_Y - 14, CAB_DIAL_X + 644 + 14,
              CAB_DIAL_Y + 428 + 14, 16);
  rrect(s, CAB_DIAL_X - 4, CAB_DIAL_Y - 4, CAB_DIAL_X + 644 + 4, CAB_DIAL_Y + 428 + 4,
        6, BRASS_DARK, 0, 0);

  /* recessed slot for the keys, brass framed */
  rrect(s, SLOT_X0, SLOT_Y0, SLOT_X1, SLOT_Y1, 10, 0x140a05, 3, BRASS_COL);
  return s;
}

/* symbol of key k, centred at cx,cy */
static void draw_symbol(SDL_Surface *s, int k, double cx, double cy, uint32_t col)
{
  double t = 9;  /* half size of arrows */

  switch (k) {
  case CAB_KEY_VOLUP:
  case CAB_KEY_VOLDOWN:
    /* speaker + plus/minus */
    rrect(s, cx - 17, cy - 5, cx - 11, cy + 5, 1, col, 0, 0);
    tri(s, cx - 12, cy - 5, cx - 3, cy - 12, cx - 3, cy + 12, col);
    tri(s, cx - 12, cy + 5, cx - 3, cy + 12, cx - 12, cy - 5, col);
    rrect(s, cx + 3, cy - 1.8, cx + 17, cy + 1.8, 1, col, 0, 0);
    if (k == CAB_KEY_VOLUP)
      rrect(s, cx + 8.2, cy - 7, cx + 11.8, cy + 7, 1, col, 0, 0);
    break;
  case CAB_KEY_PLAY:
    tri(s, cx - 15, cy - 10, cx - 15, cy + 10, cx - 1, cy, col);
    rrect(s, cx + 4, cy - 10, cx + 8, cy + 10, 1, col, 0, 0);
    rrect(s, cx + 12, cy - 10, cx + 16, cy + 10, 1, col, 0, 0);
    break;
  case CAB_KEY_RIGHT:  /* scan right: >> */
    tri(s, cx - 12, cy - t, cx - 12, cy + t, cx, cy, col);
    tri(s, cx, cy - t, cx, cy + t, cx + 12, cy, col);
    break;
  case CAB_KEY_LEFT:   /* scan left: << */
    tri(s, cx + 12, cy - t, cx + 12, cy + t, cx, cy, col);
    tri(s, cx, cy - t, cx, cy + t, cx - 12, cy, col);
    break;
  case CAB_KEY_UP:     /* next page */
    tri(s, cx - 11, cy + 6, cx + 11, cy + 6, cx, cy - 8, col);
    break;
  case CAB_KEY_DOWN:   /* previous page */
    tri(s, cx - 11, cy - 6, cx + 11, cy - 6, cx, cy + 8, col);
    break;
  }
}

/*
 * One Bakelite key: mottled brown, slightly domed (lighter top, darker
 * bottom), thin brass rim. The symbol is engraved and filled with aged
 * ivory: a dark cut above, a light lip below, then the inlay.
 */
static SDL_Surface *draw_key(int k, int down)
{
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, KEY_W, KEY_H + PRESS_DY, 32,
                                                  SDL_PIXELFORMAT_ARGB8888);
  double dy = down ? PRESS_DY : 0;
  double bottom = KEY_H - 6 + dy + (down ? 3 : 0);
  double cx = KEY_W / 2.0, cy = (KEY_H - 6) / 2.0 + dy + 2;
  double dim = down ? 0.78 : 1.0;
  int x, y;

  if (s == NULL)
    return NULL;
  SDL_FillRect(s, NULL, 0);
  rrect(s, 0, dy + 4, KEY_W, KEY_H + dy, 9, 0x140904, 0, 0);          /* depth */
  for (y = 0; y < KEY_H + PRESS_DY; y++)
    for (x = 0; x < KEY_W; x++) {
      double d = sd_rrect(x + 0.5, y + 0.5, 0, dy, KEY_W, bottom, 9);
      double v, m, dome, hl;
      if (d > 1)
        continue;
      v = (y - dy) / (bottom - dy);                                    /* 0 top .. 1 bottom */
      m = fbm(x * 0.09 + k * 13.0, y * 0.09) - 0.5;                     /* mottling */
      dome = 1.18 - 0.42 * v;
      hl = 0.22 * exp(-pow((x - KEY_W * 0.38) / 16.0, 2) - pow((v - 0.16) / 0.09, 2));
      blend(s, x, y, rgbf((70 + 40 * m) * dome * dim + 255 * hl * dim,
                          (40 + 22 * m) * dome * dim + 220 * hl * dim,
                          (22 + 12 * m) * dome * dim + 170 * hl * dim), 0.5 - d);
      if (d > -2.2)                                                    /* brass rim */
        blend(s, x, y, down ? BRASS_DARK : BRASS_COL, (0.5 - d) * (d > -1.2 ? 0.9 : 0.4));
    }
  draw_symbol(s, k, cx, cy - 1, 0x120804);                             /* cut */
  draw_symbol(s, k, cx, cy + 1, rgbf(150 * dim, 110 * dim, 75 * dim)); /* lower lip */
  draw_symbol(s, k, cx, cy, rgbf(222 * dim, 206 * dim, 165 * dim));    /* inlay */
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
