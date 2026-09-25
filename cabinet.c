#define _DEFAULT_SOURCE  /* M_PI */
/*
 * RetroWebRadio - optional radio cabinet around the dial
 *
 * A post-war table radio in the style of around 1950: box-shaped walnut
 * case with rounded top edges, a wide light veneer frame around a darker
 * front panel holding the dial, a figured lower panel with a row of small
 * ivory push buttons with printed labels, a pilot lamp on the left, a
 * magic eye tuning indicator on the right and two Bakelite knobs that
 * work as step buttons (left / right, repeating while held).
 *
 *   (knob) (lamp) [Spielen][Lauter][Leiser][<<Suche][Band+][Band-][Suche>>] (eye) (knob)
 *
 * Everything is rendered once with anti-aliased shapes and procedural
 * textures into textures; per frame only textures are copied.
 *
 * GNU GENERAL PUBLIC LICENSE Version 3
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SDL_ttf.h>
#include "cabinet.h"
#include "logo.h"

#define OUTSIDE     0x141414
#define EDGE_COL    0x24100a

#define FRAME       100                      /* width of the light frame */
#define BODY_BOTTOM (CAB_H - 18)
#define RECESS_X0   (FRAME - 22)
#define RECESS_X1   (CAB_W - FRAME + 22)
#define RECESS_Y0   (CAB_DIAL_Y - 22)
#define RECESS_Y1   (BODY_BOTTOM - 36)
#define PANEL_Y0    (CAB_DIAL_Y + 428 + 18)  /* lower, figured panel */

#define KEY_W       66
#define KEY_H       34
#define KEY_GAP     5
#define PRESS_DY    3
#define KEYS_Y      (PANEL_Y0 + 22)
#define LAMP_X      (CAB_W / 2 - (CAB_KEYS * (KEY_W + KEY_GAP)) / 2 - 30)
#define LAMP_Y      (KEYS_Y + KEY_H / 2)
#define LAMP_R      12
#define EYE_X       (CAB_W - LAMP_X + 8)
#define EYE_R       22
#define EYE_STEPS   24                        /* pre-rendered shadow widths */
#define KNOB_R      30
#define KNOB_Y      (RECESS_Y1 - 8)
#define KNOB_X(k)   ((k) ? RECESS_X1 - 14 : RECESS_X0 + 14)
#define KNOB_S      (2 * (KNOB_R + 10))       /* knob texture size */
#define LAMP_S      64
#define EYE_S       64

static SDL_Texture *cab_tex, *key_tex[CAB_KEYS][2], *lamp_tex[3], *knob_tex[2][2];
static SDL_Texture *eye_tex[EYE_STEPS + 1];

static const char *key_label[CAB_KEYS] = {
  "Spielen", "Lauter", "Leiser", "\xc2\xabSuche", "Band+", "Band-", "Suche\xc2\xbb"
};

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




/*
 * Veneer photographed on a real radio (the BMPs in textures/, see
 * textures/mktextures.py). Sampled with mirrored repetition, which also
 * gives the book-matched look. If a file is missing, the procedural
 * veneer below is used for that part.
 */
struct photo_tex {
  int w, h;
  uint32_t *px;   /* ARGB8888 */
};
static struct photo_tex photo[3];
#define TEX_SCALE 0.7   /* texels per logical pixel */

static void load_photo_tex(int tone, const char *path)
{
  SDL_Surface *raw, *conv;
  int y;

  if (path == NULL || (raw = SDL_LoadBMP(path)) == NULL)
    return;
  conv = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_ARGB8888, 0);
  SDL_FreeSurface(raw);
  if (conv == NULL)
    return;
  photo[tone].px = malloc((size_t)conv->w * conv->h * 4);
  if (photo[tone].px != NULL) {
    photo[tone].w = conv->w;
    photo[tone].h = conv->h;
    for (y = 0; y < conv->h; y++)
      memcpy(photo[tone].px + (size_t)y * conv->w,
             (uint8_t *)conv->pixels + y * conv->pitch, (size_t)conv->w * 4);
  }
  SDL_FreeSurface(conv);
}

static double mirror(double u, int n)
{
  double m = fmod(u, 2.0 * (n - 1));
  if (m < 0)
    m += 2.0 * (n - 1);
  return m > n - 1 ? 2.0 * (n - 1) - m : m;
}

/* bilinear, mirrored repeat */
static uint32_t photo_sample(const struct photo_tex *t, double u, double v)
{
  double x = mirror(u, t->w), y = mirror(v, t->h), fx, fy, c[3] = { 0, 0, 0 };
  int x0 = (int)x, y0 = (int)y, x1, y1, i, j, sh;

  x1 = x0 + 1 < t->w ? x0 + 1 : x0;
  y1 = y0 + 1 < t->h ? y0 + 1 : y0;
  fx = x - x0;
  fy = y - y0;
  for (j = 0; j < 2; j++)
    for (i = 0; i < 2; i++) {
      uint32_t p = t->px[(j ? y1 : y0) * t->w + (i ? x1 : x0)];
      double wgt = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
      for (sh = 0; sh < 3; sh++)
        c[sh] += wgt * ((p >> (16 - 8 * sh)) & 0xff);
    }
  return rgbf(c[0], c[1], c[2]);
}

/* rounded rect with separate top / bottom corner radius */
static double sd_box(double px, double py, double x0, double y0, double x1, double y1,
                     double rtop, double rbot)
{
  return sd_rrect(px, py, x0, y0, x1, y1, py < (y0 + y1) / 2 ? rtop : rbot);
}

/*
 * Walnut veneer. tone: 0 = light golden frame, 1 = dark front panel,
 * 2 = figured (crotch) lower panel. Book-matched at the centre line.
 */
static uint32_t veneer(double px, double py, int tone)
{
  double mx = fabs(px - CAB_W / 2.0), warp, fig, lines, pores, t, k;
  static const double base[3][3] = {
    { 120, 70, 34 }, { 66, 34, 16 }, { 92, 50, 24 }
  };

  if (photo[tone].px != NULL)  /* real veneer; book-matched at the centre */
    return photo_sample(&photo[tone], mx * TEX_SCALE,
                        (tone == 2 ? py - PANEL_Y0 : py) * TEX_SCALE);

  if (tone == 2) {  /* flame figure rising from the centre */
    warp = fbm(mx * 0.02, py * 0.02) - 0.5;
    lines = sin(py * 0.25 + mx * mx * 0.0009 + warp * 10);
    fig = fbm(mx * 0.03 + 3, py * 0.03);
  } else {          /* straight, slightly wavy grain */
    warp = fbm(mx * 0.004 + 7.3 * tone, py * 0.003) - 0.5;
    lines = sin(mx * 0.30 + warp * 20 + fbm(mx * 0.03, py * 0.006) * 8);
    fig = fbm(mx * 0.010 + warp * 2.0, py * 0.0022 + 3.1 * tone);
  }
  lines = pow(0.5 + 0.5 * lines, 5);
  pores = vnoise(px * 0.9, py * 0.08);
  t = 0.75 + 0.5 * fig;
  k = (1.0 - 0.25 * lines - 0.06 * pores) * t;
  return rgbf(base[tone][0] * k, base[tone][1] * k, base[tone][2] * k);
}

/* fill a shape given by sd_box with veneer */
static void veneer_box(SDL_Surface *s, double x0, double y0, double x1, double y1,
                       double rtop, double rbot, int tone)
{
  int x, y;
  for (y = (int)y0 - 1; y <= (int)y1 + 1; y++)
    for (x = (int)x0 - 1; x <= (int)x1 + 1; x++) {
      double d = sd_box(x + 0.5, y + 0.5, x0, y0, x1, y1, rtop, rbot);
      if (d < 1)
        blend(s, x, y, veneer(x + 0.5, y + 0.5, tone), 0.5 - d);
    }
}

/* soft shadow inside a recess (top/left darker, bottom/right lighter) */
static void recess_shade(SDL_Surface *s, double x0, double y0, double x1, double y1, double r)
{
  int x, y;
  for (y = (int)y0; y <= (int)y1; y++)
    for (x = (int)x0; x <= (int)x1; x++) {
      double d = -sd_rrect(x + 0.5, y + 0.5, x0, y0, x1, y1, r);   /* depth inside */
      if (d > 0 && d < 10) {
        double top = (y - y0) < (y1 - y) ? 1 : 0.35;
        blend(s, x, y, 0x000000, (1 - d / 10) * 0.45 * top);
      }
    }
}

/* brushed brass: light upper bevel, darker below, fine streaks */
static uint32_t brass(double v, double px, double py)
{
  double n = vnoise(px * 0.05, py * 1.7) - 0.5, t;
  v = v < 0 ? 0 : v > 1 ? 1 : v;
  t = 1.22 - 0.55 * v + 0.10 * n;
  if (v < 0.12)
    t += 0.30 * (1 - v / 0.12);
  return rgbf(172 * t, 134 * t, 60 * t);
}

/*
 * Maker's badge at the top centre: brass frame with two screws around a
 * red enamel field with the Niethammer-Audio speaker and signature
 * (masks from logo.h, traced smooth by textures/mklogo.py).
 */
static void draw_badge(SDL_Surface *s)
{
  const int fw = LOGO_W, fh = LOGO_H, fx0 = (CAB_W - LOGO_W) / 2, fy0 = 17;
  const double bx0 = fx0 - 7, by0 = fy0 - 7, bx1 = fx0 + fw + 7, by1 = fy0 + fh + 7;
  int x, y, k;

  /* shadow on the wood, brass frame, dark inner edge */
  rrect(s, bx0 + 1, by0 + 3, bx1 + 1, by1 + 3, 9, 0x000000, 0, 0);
  for (y = (int)by0 - 1; y <= by1 + 1; y++)
    for (x = (int)bx0 - 1; x <= bx1 + 1; x++) {
      double d = sd_rrect(x + 0.5, y + 0.5, bx0, by0, bx1, by1, 9);
      if (d < 1) {
        blend(s, x, y, brass((y - by0) / (by1 - by0), x, y), 0.5 - d);
        blend(s, x, y, 0x4a3410, (0.5 - (fabs(d + 0.7) - 0.7)) * 0.9);
      }
    }
  rrect(s, fx0 - 1.5, fy0 - 1.5, fx0 + fw + 1.5, fy0 + fh + 1.5, 4, 0x3a2408, 0, 0);

  /* enamel field with the logo */
  for (y = 0; y < fh; y++)
    for (x = 0; x < fw; x++) {
      double v = (double)y / fh;
      double spk = logo_speaker[y * fw + x] / 255.0, sig = logo_sig[y * fw + x] / 255.0;
      double d = sd_rrect(x + 0.5, y + 0.5, 0, 0, fw, fh, 3);
      if (d > 0.5)
        continue;
      blend(s, fx0 + x, fy0 + y, rgbf(214 - 40 * v, 34 - 8 * v, 30 - 6 * v), 0.5 - d);
      blend(s, fx0 + x, fy0 + y, 0x0c0c0c, spk);
      blend(s, fx0 + x, fy0 + y, 0xf6f1e4, sig);
      /* glaze: soft reflection across the upper part */
      if (y < fh * 0.45)
        blend(s, fx0 + x, fy0 + y, 0xffffff, 0.10 * (1 - y / (fh * 0.45)));
    }

  /* two screws beside the badge */
  for (k = 0; k < 2; k++) {
    double cx = k ? bx1 + 10 : bx0 - 10, cy = (by0 + by1) / 2;
    for (y = (int)cy - 5; y <= cy + 5; y++)
      for (x = (int)cx - 5; x <= cx + 5; x++) {
        double r = hypot(x + 0.5 - cx, y + 0.5 - cy);
        if (r < 4.5)
          blend(s, x, y, brass((y - cy + 4.5) / 9, x, y), 4.5 - r > 1 ? 1 : 4.5 - r);
      }
    rrect(s, cx - 3, cy - 0.6 + (k ? 0 : 0), cx + 3, cy + 0.6, 0.5, 0x3a2408, 0, 0);
  }
}

/* signature engraved in brass below the keys */
static void draw_signature(SDL_Surface *s)
{
  int top = KEYS_Y + KEY_H + 8, bottom = RECESS_Y1 - 6;
  int x0 = (CAB_W - SIG_W) / 2, y0 = top + (bottom - top - SIG_H) / 2;
  int x, y;

  for (y = 0; y < SIG_H; y++)
    for (x = 0; x < SIG_W; x++) {
      double m = sig_mask[y * SIG_W + x] / 255.0;
      if (m <= 0)
        continue;
      blend(s, x0 + x + 1, y0 + y + 1, 0x140804, m * 0.7);   /* engraved shadow */
    }
  for (y = 0; y < SIG_H; y++)
    for (x = 0; x < SIG_W; x++) {
      double m = sig_mask[y * SIG_W + x] / 255.0;
      if (m > 0)
        blend(s, x0 + x, y0 + y, brass((double)y / SIG_H, x0 + x, y0 + y), m);
    }
}

static void key_rect(int k, int *x, int *y)
{
  int total = CAB_KEYS * KEY_W + (CAB_KEYS - 1) * KEY_GAP;
  *x = (CAB_W - total) / 2 + k * (KEY_W + KEY_GAP);
  *y = KEYS_Y;
}

static SDL_Surface *draw_case(void)
{
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, CAB_W, CAB_H, 32, SDL_PIXELFORMAT_ARGB8888);
  int x, y;

  if (s == NULL)
    return NULL;
  SDL_FillRect(s, NULL, 0xff000000u | OUTSIDE);

  /* low dark plinth */
  rrect(s, 30, BODY_BOTTOM - 6, CAB_W - 30, CAB_H - 2, 4, 0x1e0e06, 0, 0);

  /* box body: light frame veneer, rounded top edges, dark outline and
     a lighter top where the rounded edge catches the light */
  veneer_box(s, 4, 4, CAB_W - 4, BODY_BOTTOM, 64, 8, 0);
  for (y = 0; y < BODY_BOTTOM + 2; y++)
    for (x = 0; x < CAB_W; x++) {
      double d = sd_box(x + 0.5, y + 0.5, 4, 4, CAB_W - 4, BODY_BOTTOM, 64, 8);
      if (d < 1 && d > -18) {
        double e = (d + 18) / 18;                 /* 0 inside .. 1 at the edge */
        blend(s, x, y, EDGE_COL, (0.5 - d > 1 ? 1 : 0.5 - d) * 0.8 * e * e * e);
        if (y < 70)
          blend(s, x, y, 0xffe0b0, 0.10 * (1 - e) * (1 - y / 70.0));
      }
    }

  draw_badge(s);

  /* darker front panel in a recess, with its shadow */
  veneer_box(s, RECESS_X0, RECESS_Y0, RECESS_X1, RECESS_Y1, 30, 6, 1);
  recess_shade(s, RECESS_X0, RECESS_Y0, RECESS_X1, RECESS_Y1, 12);

  /* figured lower panel below the dial */
  veneer_box(s, RECESS_X0 + 6, PANEL_Y0, RECESS_X1 - 6, RECESS_Y1 - 6, 4, 4, 2);

  draw_signature(s);

  /* thin gold strip around the dial and above the keys */
  rrect(s, CAB_DIAL_X - 5, CAB_DIAL_Y - 5, CAB_DIAL_X + 644 + 5, CAB_DIAL_Y + 428 + 5,
        4, 0xb8914a, 1.2, 0x5a3e18);
  rrect(s, RECESS_X0 + 6, PANEL_Y0 - 4, RECESS_X1 - 6, PANEL_Y0 - 1, 1, 0xb8914a, 0, 0);

  /* dark slot for the key row */
  {
    int kx, ky, kx2, ky2;
    key_rect(0, &kx, &ky);
    key_rect(CAB_KEYS - 1, &kx2, &ky2);
    rrect(s, kx - 6, ky - 6, kx2 + KEY_W + 6, ky + KEY_H + 8, 5, 0x100804, 1.5, 0x3a2412);
  }

  /* lamp socket and the metal ring of the magic eye */
  rrect(s, LAMP_X - LAMP_R - 4, LAMP_Y - LAMP_R - 4, LAMP_X + LAMP_R + 4, LAMP_Y + LAMP_R + 4,
        LAMP_R + 4, 0x1a0d06, 2.5, 0x6a4a28);
  for (y = LAMP_Y - EYE_R - 6; y <= LAMP_Y + EYE_R + 6; y++)
    for (x = EYE_X - EYE_R - 6; x <= EYE_X + EYE_R + 6; x++) {
      double r = hypot(x + 0.5 - EYE_X, y + 0.5 - LAMP_Y);
      double t = 1.15 - 0.5 * (y + 0.5 - LAMP_Y + EYE_R) / (2.0 * EYE_R);
      if (r < EYE_R + 5)
        blend(s, x, y, rgbf(150 * t, 140 * t, 120 * t), EYE_R + 5 - r > 1 ? 1 : EYE_R + 5 - r);
    }
  return s;
}

/* key with printed label; 'down' = pressed */
static SDL_Surface *draw_key(int k, int down, TTF_Font *font)
{
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, KEY_W, KEY_H + PRESS_DY + 2, 32,
                                                  SDL_PIXELFORMAT_ARGB8888);
  double dy = down ? PRESS_DY : 0, bottom = KEY_H + dy - (down ? 0 : 3);
  double dim = down ? 0.88 : 1.0;
  int x, y;

  if (s == NULL)
    return NULL;
  SDL_FillRect(s, NULL, 0);
  rrect(s, 0, dy + 3, KEY_W, KEY_H + dy + 1, 4, 0x8a7a5c, 0, 0);   /* key side */
  for (y = 0; y < s->h; y++)
    for (x = 0; x < KEY_W; x++) {
      double d = sd_rrect(x + 0.5, y + 0.5, 0.5, dy, KEY_W - 0.5, bottom, 4);
      double v = (y - dy) / (bottom - dy), m;
      if (d > 1)
        continue;
      m = vnoise(x * 0.4 + k * 17, y * 0.4) - 0.5;
      blend(s, x, y, rgbf((238 - 30 * v + 6 * m) * dim, (228 - 34 * v + 6 * m) * dim,
                          (200 - 40 * v + 5 * m) * dim), 0.5 - d);
      if (d > -1.3)
        blend(s, x, y, 0x6a5a40, (0.5 - d) * 0.6);
    }
  if (font != NULL) {
    SDL_Color c = { 60, 40, 24, 255 };
    SDL_Surface *t = TTF_RenderUTF8_Blended(font, key_label[k], c);
    if (t != NULL) {
      SDL_Rect r = { (KEY_W - t->w) / 2, (int)(dy + (bottom - dy - t->h) / 2), 0, 0 };
      SDL_SetSurfaceBlendMode(t, SDL_BLENDMODE_BLEND);
      SDL_BlitSurface(t, NULL, s, &r);
      SDL_FreeSurface(t);
    }
  }
  return s;
}

/* fluted Bakelite knob with shadow; 'down' = pressed (sunk, darker) */
static SDL_Surface *draw_knob(int k, int down)
{
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, KNOB_S, KNOB_S, 32, SDL_PIXELFORMAT_ARGB8888);
  double c = KNOB_S / 2.0, cy = c - 3 + (down ? 2 : 0), dim = down ? 0.8 : 1.0;
  int x, y;

  if (s == NULL)
    return NULL;
  SDL_FillRect(s, NULL, 0);
  for (y = 0; y < KNOB_S; y++)
    for (x = 0; x < KNOB_S; x++) {
      double dx = x + 0.5 - c, dy = y + 0.5 - cy, r = hypot(dx, dy);
      double ang = atan2(dy, dx), flute = 1.2 * cos(ang * 24);
      double ds = hypot(dx, dy - (down ? 2 : 4)) - KNOB_R;          /* shadow */
      double d = r - (KNOB_R + flute * (r > KNOB_R - 5 ? 1 : 0));
      double lit = (1.0 - 0.45 * (dy / KNOB_R) - 0.15 * (dx / KNOB_R)) * dim;
      double m = fbm(x * 0.12 + k * 9, y * 0.12) - 0.5;
      double cap = r < KNOB_R - 9 ? 1.15 : 1.0;

      blend(s, x, y, 0x000000, (0.5 - ds / 4) * (down ? 0.3 : 0.45));
      blend(s, x, y, rgbf((34 + 14 * m) * lit * cap, (18 + 8 * m) * lit * cap,
                          (10 + 5 * m) * lit * cap), 0.5 - d);
      if (r < KNOB_R - 9 && r > KNOB_R - 11)                         /* cap edge */
        blend(s, x, y, 0x000000, 0.35);
    }
  rrect(s, c - 12, cy - 18, c + 2, cy - 12, 3, down ? 0x6a4a30 : 0x9a6a44, 0, 0); /* gloss */
  return s;
}

/* pilot lamp glass: 0 dark, 1 warm light, 2 red */
static SDL_Surface *draw_lamp(int state)
{
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, LAMP_S, LAMP_S, 32, SDL_PIXELFORMAT_ARGB8888);
  double c = LAMP_S / 2.0;
  int x, y;

  if (s == NULL)
    return NULL;
  SDL_FillRect(s, NULL, 0);
  for (y = 0; y < LAMP_S; y++)
    for (x = 0; x < LAMP_S; x++) {
      double r = hypot(x + 0.5 - c, y + 0.5 - c), g = 1 - r / (LAMP_R + 0.5);
      uint32_t col;
      if (state == 1)
        blend(s, x, y, 0xff9030, 0.40 * exp(-pow(r / (LAMP_R * 1.5), 2)));   /* glow */
      else if (state == 2)
        blend(s, x, y, 0xff2010, 0.40 * exp(-pow(r / (LAMP_R * 1.5), 2)));
      if (r >= LAMP_R)
        continue;
      col = state == 1 ? rgbf(255, 120 + 120 * g, 40 + 140 * g * g)
          : state == 2 ? rgbf(255, 40 + 150 * g * g, 20 + 110 * g * g)
          : rgbf(80 + 40 * g, 28 + 14 * g, 14 + 6 * g);
      blend(s, x, y, col, LAMP_R - r > 1 ? 1 : LAMP_R - r);
      /* small reflection on the glass */
      if (hypot(x + 0.5 - (c - 4), y + 0.5 - (c - 4)) < 2.5)
        blend(s, x, y, 0xffffff, state ? 0.5 : 0.35);
    }
  return s;
}

/*
 * Magic eye (tuning indicator tube, seen from the front): green
 * fluorescent ring around a dark cap, with two dark shadow sectors at the
 * top and bottom. open = 1: detuned, wide sectors; open = 0: tuned in,
 * the sectors are closed.
 */
static SDL_Surface *draw_eye(double open)
{
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, EYE_S, EYE_S, 32, SDL_PIXELFORMAT_ARGB8888);
  double c = EYE_S / 2.0, half = 4 + open * 50;   /* half width of a sector, degrees */
  int x, y;

  if (s == NULL)
    return NULL;
  SDL_FillRect(s, NULL, 0);
  for (y = 0; y < EYE_S; y++)
    for (x = 0; x < EYE_S; x++) {
      double dx = x + 0.5 - c, dy = y + 0.5 - c, r = hypot(dx, dy);
      double deg = fabs(atan2(dx, -dy)) * 180 / M_PI;   /* 0 = up, 180 = down */
      double off = deg < 90 ? deg : 180 - deg;           /* distance to the axis */
      double edge, glow, a;
      if (r > EYE_R)
        continue;
      a = EYE_R - r > 1 ? 1 : EYE_R - r;
      blend(s, x, y, 0x06140a, a);                       /* dark glass */
      if (r > 7) {
        edge = (off - half) / 2.5;                       /* soft shadow edge */
        glow = edge > 1 ? 1 : edge < 0 ? 0 : edge;
        glow *= 0.55 + 0.45 * (r - 7) / (EYE_R - 7);     /* brighter outside */
        blend(s, x, y, rgbf(60 + 120 * glow, 255 * glow, 90 + 60 * glow), a * glow);
      }
      if (r < 7)                                         /* cathode cap */
        blend(s, x, y, 0x101010, 7 - r > 1 ? 1 : 7 - r);
    }
  return s;
}

/* a serif face if the system has one, else the program's font */
static TTF_Font *label_font(const char *fallback)
{
  static const char *serif[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf",
    "/usr/share/fonts/TTF/DejaVuSerif-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSerif-Bold.ttf",
    NULL
  };
  TTF_Font *f;
  int i;

  for (i = 0; serif[i] != NULL; i++)
    if ((f = TTF_OpenFont(serif[i], 11)) != NULL)
      return f;
  return fallback ? TTF_OpenFont(fallback, 10) : NULL;
}

/* surface -> blended texture, frees the surface */
static SDL_Texture *to_texture(SDL_Renderer *r, SDL_Surface *s)
{
  SDL_Texture *t;
  if (s == NULL)
    return NULL;
  t = SDL_CreateTextureFromSurface(r, s);
  if (t != NULL)
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  SDL_FreeSurface(s);
  return t;
}

int cabinet_init(SDL_Renderer *r, const char *font_path, const char *const tex_path[3])
{
  SDL_Surface *s;
  TTF_Font *font;
  int k, d;

  if (cab_tex != NULL)
    return 0;
  for (k = 0; k < 3 && tex_path != NULL; k++)
    load_photo_tex(k, tex_path[k]);
  if ((s = draw_case()) == NULL)
    return -1;
  cab_tex = SDL_CreateTextureFromSurface(r, s);
  SDL_FreeSurface(s);
  for (k = 0; k < 3; k++) {  /* only needed while drawing the case */
    free(photo[k].px);
    photo[k].px = NULL;
  }

  font = label_font(font_path);
  for (k = 0; k < CAB_KEYS; k++)
    for (d = 0; d < 2; d++) {
      SDL_Surface *ks = draw_key(k, d, font);
      if (ks == NULL)
        return -1;
      key_tex[k][d] = SDL_CreateTextureFromSurface(r, ks);
      SDL_SetTextureBlendMode(key_tex[k][d], SDL_BLENDMODE_BLEND);
      SDL_FreeSurface(ks);
    }
  if (font != NULL)
    TTF_CloseFont(font);
  for (d = 0; d < 3; d++)
    lamp_tex[d] = to_texture(r, draw_lamp(d));
  for (k = 0; k < 2; k++)
    for (d = 0; d < 2; d++)
      knob_tex[k][d] = to_texture(r, draw_knob(k, d));
  for (k = 0; k <= EYE_STEPS; k++)
    eye_tex[k] = to_texture(r, draw_eye((double)k / EYE_STEPS));
  return cab_tex != NULL ? 0 : -1;
}

void cabinet_render(SDL_Renderer *r, SDL_Texture *dial, int pressed, unsigned latched,
                    int lamp, double eye_open)
{
  SDL_Rect dst = { CAB_DIAL_X, CAB_DIAL_Y, 644, 428 };
  SDL_Rect lr = { LAMP_X - LAMP_S / 2, LAMP_Y - LAMP_S / 2, LAMP_S, LAMP_S };
  SDL_Rect er = { EYE_X - EYE_S / 2, LAMP_Y - EYE_S / 2, EYE_S, EYE_S };
  int k, e;

  SDL_RenderCopy(r, cab_tex, NULL, NULL);
  SDL_RenderCopy(r, dial, NULL, &dst);
  if (lamp < 0 || lamp > 2)
    lamp = 0;
  if (lamp_tex[lamp] != NULL)
    SDL_RenderCopy(r, lamp_tex[lamp], NULL, &lr);
  e = (int)lround((eye_open < 0 ? 0 : eye_open > 1 ? 1 : eye_open) * EYE_STEPS);
  if (eye_tex[e] != NULL)
    SDL_RenderCopy(r, eye_tex[e], NULL, &er);
  for (k = 0; k < 2; k++) {
    SDL_Rect kr = { KNOB_X(k) - KNOB_S / 2, KNOB_Y - KNOB_S / 2 + 3, KNOB_S, KNOB_S };
    int down = pressed == (k ? CAB_HIT_KNOB_RIGHT : CAB_HIT_KNOB_LEFT);
    if (knob_tex[k][down] != NULL)
      SDL_RenderCopy(r, knob_tex[k][down], NULL, &kr);
  }
  for (k = 0; k < CAB_KEYS; k++) {
    SDL_Rect kr;
    int down = (k == pressed) || (latched & (1u << k));
    key_rect(k, &kr.x, &kr.y);
    kr.w = KEY_W;
    kr.h = KEY_H + PRESS_DY + 2;
    SDL_RenderCopy(r, key_tex[k][down], NULL, &kr);
  }
}

SDL_Surface *cabinet_mask(void)
{
  SDL_Surface *m = SDL_CreateRGBSurfaceWithFormat(0, CAB_W, CAB_H, 32, SDL_PIXELFORMAT_ARGB8888);
  int x, y;

  if (m == NULL)
    return NULL;
  for (y = 0; y < CAB_H; y++)
    for (x = 0; x < CAB_W; x++) {
      int in = sd_box(x + 0.5, y + 0.5, 4, 4, CAB_W - 4, BODY_BOTTOM, 64, 8) < 0.5 ||
               (x >= 30 && x < CAB_W - 30 && y >= BODY_BOTTOM - 6 && y < CAB_H - 2);
      ((uint32_t *)((uint8_t *)m->pixels + y * m->pitch))[x] = in ? 0xff000000u : 0;
    }
  return m;
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
  for (k = 0; k < 2; k++)
    if (hypot(x - KNOB_X(k), y - KNOB_Y) <= KNOB_R + 4)
      return k ? CAB_HIT_KNOB_RIGHT : CAB_HIT_KNOB_LEFT;
  return CAB_HIT_NONE;
}
