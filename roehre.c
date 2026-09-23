/*
 * Raspberry pi retro radio
 * (C) 2023 Amrhein & Niethammer
 * GNU GENERAL PUBLIC LICENSE Version 3
 *
 * most significant changes:
 * UN 2016 removed parser with memory leaks and included xml parser
 * UN 2016 potentiometer deamon for raspberry pi 2/3 B+
 * 2026 port to SDL2, stations loaded once, mpc called without a shell,
 *      fixed tuning/timer bugs, no more busy loop
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

// sudo apt-get install libsdl2-dev libsdl2-ttf-dev libxml2-dev mpd mpc

#define _POSIX_C_SOURCE 200809L
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "stations.h"
#include "radio_icon.h"

#define STATSPERCOL 10      // maximal stations per column
#define WIN_WIDTH 644       // width of screen, 644/14=46 grid
#define WIN_HEIGHT 428      // height of screen
#define VISIBLE_WIDTH 500   // width of visible window on screen
#define VISIBLE_HEIGHT 428  // height of visible window on screen
#define OFFSET_X 80         // horizontal offset of visible window
#define OFFSET_Y 0          // vertical offset of visible window
#define HIGHLIGHT_XWINDOW 10 // tuner catches a station within +-10 px
#define GRID_STEP 46

#define TICK_MS 20          // main loop period
#define IDLE_REDRAW_MS 500  // redraw at least this often
#define FRAME_MS 16         // frame period while the title scrolls
#define SCROLL_SPEED 45     // title scrolling in px per second
#define SCROLL_GAP 60       // px between end and restart of a scrolling title
#define SCROLL_PAUSE_MS 1500 // show the start of a new title before scrolling
#define TRACK_Y (VISIBLE_HEIGHT - 20 + OFFSET_Y)
#define TRACK_POLL_MS 1000  // ask mpc for the current title
#define TUNE_DELAY_MS 1000  // station must stay tuned this long before it plays
#define SCAN_STEP 2         // px per tick while auto-scanning
#define KEY_STEP 5          // px per cursor key press
#define TRACK_MAX 512
#define SAVE_DELAY_MS 2000  // save the dial position once it has settled

#ifndef DATADIR
#define DATADIR "/usr/local/share/retrowebradio"
#endif

static const char FONTFILE[] = "VeraMono.ttf";
static const char STATIONSFILE[] = "stations.xml";

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_Surface *screen;           // software canvas, WIN_WIDTH x WIN_HEIGHT
static TTF_Font *station_font;
static TTF_Font *station_font_big;
static TTF_Font *track_font;
static struct stationlist stations;

static int xpos = 0;                  // tuner position relative to OFFSET_X
static int current_page = 1;          // 1 .. stations.pages
static int search_dir = 0;            // auto-scan: +1 right, -1 left, 0 off

static const struct station *tuned;   // station under the tuner or NULL
static uint64_t tuned_since;
static char current_playing_url[STATION_URL_MAX];
static pid_t tune_pid = 0;            // child running the mpc sequence

static char current_track[TRACK_MAX];
static SDL_Surface *background;       // grid, stations, tuner (cached)
static SDL_Surface *target;           // surface the draw_* helpers paint on
static SDL_Surface *track_surf;       // rendered bottom line (cached)
static char track_text[TRACK_MAX];    // text of track_surf
static uint64_t scroll_start;
static char toast[128];               // short message at the bottom (volume, station)
static uint64_t toast_timeout = 0;
static int refresh_now = 1;
static volatile sig_atomic_t stop_requested = 0;
enum { WIN_NONE, WIN_SHOW, WIN_HIDE, WIN_TOGGLE };
static volatile sig_atomic_t window_request = WIN_NONE;

/* tray icon: SIGUSR1 show, SIGUSR2 hide, SIGRTMIN toggle */
static void on_window_signal(int sig)
{
  if (sig == SIGUSR1)
    window_request = WIN_SHOW;
  else if (sig == SIGUSR2)
    window_request = WIN_HIDE;
  else
    window_request = WIN_TOGGLE;
}

static void on_signal(int sig)
{
  (void)sig;
  stop_requested = 1;  /* leave the main loop, save the position, exit */
}

/* monotonic milliseconds; 64 bit, never wraps */
static uint64_t now_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static void show_toast(const char *msg, unsigned ms)
{
  snprintf(toast, sizeof(toast), "%s", msg);
  toast_timeout = now_ms() + ms;
  refresh_now = 1;
}

/* ------------------------------------------------------------------ */
/* running mpc without a shell                                         */
/* ------------------------------------------------------------------ */

/* fork+exec and wait; only used inside helper children */
static int run_wait(char *const argv[])
{
  pid_t pid = fork();
  int status;

  if (pid < 0)
    return -1;
  if (pid == 0) {
    int fd = open("/dev/null", O_WRONLY);
    if (fd >= 0) {
      dup2(fd, STDOUT_FILENO);
      close(fd);
    }
    execvp(argv[0], argv);
    _exit(127);
  }
  while (waitpid(pid, &status, 0) < 0)
    if (errno != EINTR)
      return -1;
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* fire and forget; the child is reaped by reap_children() */
static pid_t run_async(char *const argv[])
{
  pid_t pid = fork();

  if (pid < 0) {
    perror("fork");
    return -1;
  }
  if (pid == 0) {
    setpgid(0, 0);
    execvp(argv[0], argv);
    _exit(127);
  }
  return pid;
}

static void mpc_volume(const char *delta)
{
  char *argv[] = { "mpc", "-q", "volume", (char *)delta, NULL };
  run_async(argv);
}

/* current mpd volume in percent, -1 if unknown (mpd down or no mixer) */
static int mpc_get_volume(void)
{
  char line[128];
  int vol = -1;
  FILE *fp = popen("mpc volume 2>/dev/null", "r");

  if (fp == NULL)
    return -1;
  if (fgets(line, sizeof(line), fp) != NULL)
    if (sscanf(line, "volume: %d", &vol) != 1)
      vol = -1;
  pclose(fp);
  return vol;
}

/* mute: remember the volume and set 0; unmute: restore it */
static void mpc_toggle_mute(void)
{
  static int saved = 0;
  char arg[16], msg[32];
  int vol = mpc_get_volume();
  char *argv[] = { "mpc", "-q", "volume", arg, NULL };

  if (vol < 0) {
    show_toast("No volume control", 1000);
    return;
  }
  if (vol > 0) {
    saved = vol;
    snprintf(arg, sizeof(arg), "0");
    snprintf(msg, sizeof(msg), "Mute");
  } else {
    snprintf(arg, sizeof(arg), "%d", saved > 0 ? saved : 50);
    snprintf(msg, sizeof(msg), "Volume %s%%", arg);
  }
  run_async(argv);
  show_toast(msg, 800);
}

/*
 * Tune to url (or silence if url is empty). The three mpc calls must run
 * in order, so they run sequentially in one child. A still running
 * previous sequence is killed first, so two tunings never interleave.
 */
static void mpc_tune(const char *url)
{
  pid_t pid;

  if (tune_pid > 0) {
    kill(-tune_pid, SIGTERM);  /* whole process group incl. a running mpc */
  }
  pid = fork();
  if (pid < 0) {
    perror("fork");
    return;
  }
  if (pid == 0) {
    char *clear[] = { "mpc", "-q", "clear", NULL };
    char *add[]   = { "mpc", "-q", "add", (char *)url, NULL };
    char *play[]  = { "mpc", "-q", "play", NULL };

    setpgid(0, 0);
    signal(SIGTERM, SIG_DFL);
    if (run_wait(clear) != 0)
      fprintf(stderr, "mpc clear failed\n");
    if (url[0] != '\0') {
      if (run_wait(add) != 0)
        fprintf(stderr, "mpc add %s failed\n", url);
      else
        run_wait(play);
    }
    _exit(0);
  }
  setpgid(pid, pid);  /* also set from the parent to avoid a race */
  tune_pid = pid;
}

static void reap_children(void)
{
  pid_t pid;
  int status;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    if (pid == tune_pid)
      tune_pid = 0;
}

/* ------------------------------------------------------------------ */
/* station geometry                                                    */
/* ------------------------------------------------------------------ */

static const struct page *page_now(void)
{
  return &stations.page[current_page - 1];
}

/* x of the center of station i (0 based) in screen coordinates */
static int station_x(const struct page *p, int i)
{
  return OFFSET_X + i * ((VISIBLE_WIDTH - 60) / p->count) + 30;
}

static int station_y(int i)
{
  int rows = STATSPERCOL >= 2 ? STATSPERCOL - 1 : 1;
  return OFFSET_Y + (i % STATSPERCOL) * ((VISIBLE_HEIGHT - 120) / rows) + 35;
}

static int tuner_x(void)
{
  return xpos + OFFSET_X;
}

static int is_highlighted(const struct page *p, int i)
{
  int x = station_x(p, i);
  return x > tuner_x() - HIGHLIGHT_XWINDOW && x < tuner_x() + HIGHLIGHT_XWINDOW;
}

static void page_step(int dir)
{
  current_page += dir;
  if (current_page > stations.pages)
    current_page = 1;
  if (current_page < 1)
    current_page = stations.pages;
}

/* move the tuner, wrapping to the next/previous page at the edges */
static void move_tuner(int dx)
{
  xpos += dx;
  if (xpos > WIN_WIDTH - OFFSET_X) {
    xpos = -OFFSET_X;
    page_step(+1);
  } else if (xpos < -OFFSET_X) {
    xpos = WIN_WIDTH - OFFSET_X;
    page_step(-1);
  }
  refresh_now = 1;
}

/*
 * auto-scan: move until the tuner crosses a station center, then snap
 * exactly onto it and stop.
 */
static void scan_step(void)
{
  const struct page *p;
  int before, after, page_before, i;

  if (search_dir == 0)
    return;
  before = tuner_x();
  page_before = current_page;
  move_tuner(search_dir * SCAN_STEP);
  if (current_page != page_before)
    return;  /* wrapped; continue on the new page next tick */
  after = tuner_x();
  p = page_now();
  for (i = 0; i < p->count; i++) {
    int c = station_x(p, i);
    if ((search_dir > 0 && before < c && c <= after) ||
        (search_dir < 0 && after <= c && c < before)) {
      xpos = c - OFFSET_X;
      search_dir = 0;
      return;
    }
  }
}

/* find the station under the tuner and remember when it was tuned in */
static void update_tuning(void)
{
  const struct page *p = page_now();
  const struct station *s = NULL;
  int i;

  for (i = 0; i < p->count; i++)
    if (is_highlighted(p, i)) {
      s = &p->stations[i];
      break;
    }
  if (s != tuned) {
    if (s == NULL || tuned == NULL || strcmp(s->url, tuned->url) != 0) {
      tuned_since = now_ms();
      if (s != NULL)
        fprintf(stderr, "new tuned station: %s\n", s->url);
    }
    tuned = s;
    refresh_now = 1;
  }
}

/* start playback once a station has been tuned long enough */
static void play_current(void)
{
  const char *want = tuned ? tuned->url : "";

  if (strcmp(want, current_playing_url) == 0)
    return;
  if (tuned != NULL && now_ms() - tuned_since < TUNE_DELAY_MS)
    return;  /* not tuned in long enough, wait a bit more */

  snprintf(current_playing_url, sizeof(current_playing_url), "%s", want);
  fprintf(stderr, "tuning station: %s\n", want[0] ? want : "(none)");
  mpc_tune(current_playing_url);
  current_track[0] = '\0';
  if (tuned != NULL)
    show_toast(tuned->name, 1500);
}

/* ------------------------------------------------------------------ */
/* current track                                                       */
/* ------------------------------------------------------------------ */

static void chomp(char *s)
{
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
    s[--n] = '\0';
}

/* ask mpc which track is currently playing */
static void get_current_track(void)
{
  char line[1024];
  char new_track[TRACK_MAX] = "";
  FILE *fp;
  int first = 1;

  if (current_playing_url[0] == '\0') {
    if (current_track[0] != '\0') {
      current_track[0] = '\0';
      refresh_now = 1;
    }
    return;
  }

  fp = popen("mpc 2>&1", "r");
  if (fp == NULL) {
    snprintf(new_track, sizeof(new_track), "Error receiving station info");
  } else {
    while (fgets(line, sizeof(line), fp) != NULL) {
      chomp(line);
      if (strncmp(line, "ERROR", 5) == 0) {
        snprintf(new_track, sizeof(new_track), "%s", line);
        break;
      }
      if (first) {
        first = 0;
        /* status line only (nothing playing) or a bare URL: no title */
        if (strncmp(line, "volume:", 7) != 0 &&
            strncmp(line, "http", 4) != 0 &&
            strncmp(line, "mms://", 6) != 0)
          snprintf(new_track, sizeof(new_track), "%s", line);
      }
    }
    pclose(fp);
  }
  if (strcmp(current_track, new_track) != 0) {
    snprintf(current_track, sizeof(current_track), "%s", new_track);
    refresh_now = 1;
  }
}

/* ------------------------------------------------------------------ */
/* drawing                                                             */
/* ------------------------------------------------------------------ */

static void fill(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b)
{
  SDL_Rect rect = { x, y, w, h };
  SDL_FillRect(target, &rect, SDL_MapRGB(target->format, r, g, b));
}

static void blit_text(TTF_Font *font, const char *text, SDL_Color color, int x, int y)
{
  SDL_Surface *s;
  SDL_Rect rect = { x, y, 0, 0 };

  if (text[0] == '\0')
    return;  /* SDL_ttf refuses zero width text */
  s = TTF_RenderUTF8_Solid(font, text, color);
  if (s == NULL)
    return;
  SDL_BlitSurface(s, NULL, target, &rect);
  SDL_FreeSurface(s);
}

/* draw the green lines on the display */
static void draw_grid(void)
{
  int x;
  for (x = 0; x < WIN_WIDTH; x += GRID_STEP)
    fill(x, 0, 1, WIN_HEIGHT, 0x00, 0x44, 0x00);
}

static void draw_stations(void)
{
  const SDL_Color normal = { 149, 207, 50, 255 };
  const SDL_Color highlight = { 255, 255, 155, 255 };
  const struct page *p = page_now();
  int i, w, h;

  for (i = 0; i < p->count; i++) {
    int hl = is_highlighted(p, i);
    TTF_Font *font = hl ? station_font_big : station_font;
    const char *name = p->stations[i].name;

    if (TTF_SizeUTF8(font, name, &w, &h) != 0)
      continue;
    blit_text(font, name, hl ? highlight : normal,
              station_x(p, i) - w / 2, station_y(i) - (hl ? 1 : 0));
  }
}

/* draw the tuner (that red bar) */
static void draw_tuner(void)
{
  fill(tuner_x(), OFFSET_Y, 4, VISIBLE_HEIGHT, 0xff, 0x77, 0x00);
}

/*
 * Bottom line (title or toast). The text is rendered once into a cached
 * surface; a long title scrolls pixel by pixel, time based, so the speed
 * stays even regardless of frame timing.
 */
static const char *bottom_text(void)
{
  if (toast_timeout > now_ms())
    return toast;
  if (current_playing_url[0] != '\0' && tuned != NULL)
    return current_track;
  return "";
}

static void update_track_surface(void)
{
  const SDL_Color white = { 255, 255, 255, 255 };
  const char *text = bottom_text();

  if (strcmp(text, track_text) == 0)
    return;
  snprintf(track_text, sizeof(track_text), "%s", text);
  if (track_surf != NULL)
    SDL_FreeSurface(track_surf);
  track_surf = text[0] != '\0' ? TTF_RenderUTF8_Blended(track_font, text, white) : NULL;
  scroll_start = now_ms();
}

static int track_scrolls(void)
{
  update_track_surface();
  return track_surf != NULL && track_surf->w > WIN_WIDTH - 20;
}

static void draw_current_track(void)
{
  SDL_Rect dst = { 10, TRACK_Y, 0, 0 };

  update_track_surface();
  if (track_surf == NULL)
    return;
  if (track_surf->w > WIN_WIDTH - 20) {
    uint64_t el = now_ms() - scroll_start;
    int period = track_surf->w + SCROLL_GAP;

    el = el > SCROLL_PAUSE_MS ? el - SCROLL_PAUSE_MS : 0;
    dst.x = 10 - (int)((el * SCROLL_SPEED / 1000) % (uint64_t)period);
    SDL_BlitSurface(track_surf, NULL, screen, &dst);
    dst.x += period;  /* the wrapped copy following the gap */
  }
  SDL_BlitSurface(track_surf, NULL, screen, &dst);
}

/*
 * Grid, stations and tuner only change on input or tuning; they are drawn
 * into a background surface and reused. With only the bottom line
 * changing (scrolling), just that strip is copied and uploaded.
 */
static void draw_everything(int full)
{
  SDL_Rect strip = { 0, TRACK_Y, WIN_WIDTH, WIN_HEIGHT - TRACK_Y };

  if (full) {
    target = background;
    SDL_FillRect(background, NULL, SDL_MapRGB(background->format, 0, 0, 0));
    draw_grid();
    draw_stations();
    draw_tuner();
    target = screen;
    SDL_BlitSurface(background, NULL, screen, NULL);
  } else {
    SDL_Rect r = strip;
    SDL_BlitSurface(background, &strip, screen, &r);
  }
  draw_current_track();
  if (full)
    SDL_UpdateTexture(texture, NULL, screen->pixels, screen->pitch);
  else
    SDL_UpdateTexture(texture, &strip,
                      (Uint8 *)screen->pixels + strip.y * screen->pitch, screen->pitch);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

/* ------------------------------------------------------------------ */
/* events                                                              */
/* ------------------------------------------------------------------ */

/*
 * The keyboard is also how the rotary encoder daemons in radio/ talk to
 * this program (they send fake X key events).
 */
static int process_events(void)
{
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_ESCAPE:
      case SDLK_q:
        return 0;
      case SDLK_RIGHT:
        search_dir = 0;
        move_tuner(+KEY_STEP);
        break;
      case SDLK_LEFT:
        search_dir = 0;
        move_tuner(-KEY_STEP);
        break;
      case SDLK_UP:
        search_dir = 0;
        page_step(+1);
        break;
      case SDLK_DOWN:
        search_dir = 0;
        page_step(-1);
        break;
      case SDLK_r:
        search_dir = +1;
        break;
      case SDLK_l:
        search_dir = -1;
        break;
      case SDLK_v:
      case SDLK_PLUS:
      case SDLK_VOLUMEUP:
      case SDLK_KP_PLUS:
        mpc_volume("+3");
        show_toast("Volume +3", 300);
        break;
      case SDLK_m:
      case SDLK_AUDIOMUTE:
        mpc_toggle_mute();
        break;
      case SDLK_MINUS:
      case SDLK_KP_MINUS:
      case SDLK_VOLUMEDOWN:
        mpc_volume("-3");
        show_toast("Volume -3", 300);
        break;
      default:
        break;
      }
      break;
    case SDL_MOUSEBUTTONUP:
      search_dir = 0;
      xpos = event.button.x - OFFSET_X;  /* logical coordinates */
      break;
    case SDL_QUIT:
      return 0;
    default:
      break;
    }
    refresh_now = 1;
  }
  return 1;
}

/* ------------------------------------------------------------------ */
/* remember the dial position between runs                            */
/* ------------------------------------------------------------------ */

static char state_file[PATH_MAX + 16];
static int saved_page = -1, saved_xpos = 0;

/* $XDG_STATE_HOME/retrowebradio/position or ~/.local/state/... */
static void init_state_file(void)
{
  const char *base = getenv("XDG_STATE_HOME");
  const char *home = getenv("HOME");
  char dir[PATH_MAX];

  if (base != NULL && base[0] == '/')
    snprintf(dir, sizeof(dir), "%s/retrowebradio", base);
  else if (home != NULL) {
    snprintf(dir, sizeof(dir), "%s/.local", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.local/state", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.local/state/retrowebradio", home);
  } else
    return;
  mkdir(dir, 0755);
  snprintf(state_file, sizeof(state_file), "%s/position", dir);
}

static void load_position(void)
{
  FILE *fp;
  int page, x;

  if (state_file[0] == '\0' || (fp = fopen(state_file, "r")) == NULL)
    return;
  if (fscanf(fp, "%d %d", &page, &x) == 2 &&
      page >= 1 && page <= stations.pages &&
      x >= -OFFSET_X && x <= WIN_WIDTH - OFFSET_X) {
    current_page = page;
    xpos = x;
    saved_page = page;
    saved_xpos = x;
  }
  fclose(fp);
}

static void save_position(void)
{
  char tmp[PATH_MAX + 24];
  FILE *fp;

  if (state_file[0] == '\0' || (current_page == saved_page && xpos == saved_xpos))
    return;
  snprintf(tmp, sizeof(tmp), "%s.tmp", state_file);
  if ((fp = fopen(tmp, "w")) == NULL)
    return;
  fprintf(fp, "%d %d\n", current_page, xpos);
  if (fclose(fp) == 0 && rename(tmp, state_file) == 0) {
    saved_page = current_page;
    saved_xpos = xpos;
  }
}

/*
 * On start, if mpd already plays the station under the (restored) tuner,
 * take it over instead of tuning it again, which would cause a gap.
 */
static void adopt_playing_station(void)
{
  char line[STATION_URL_MAX];
  FILE *fp;

  update_tuning();
  if (tuned == NULL || (fp = popen("mpc current -f %file% 2>/dev/null", "r")) == NULL)
    return;
  if (fgets(line, sizeof(line), fp) != NULL) {
    chomp(line);
    if (strcmp(line, tuned->url) == 0) {
      snprintf(current_playing_url, sizeof(current_playing_url), "%s", line);
      fprintf(stderr, "already playing: %s\n", line);
    }
  }
  pclose(fp);
}

/*
 * Publish whether the window is visible, so the tray can offer "show" or
 * "hide": $XDG_RUNTIME_DIR/retrowebradio-visible contains 1 or 0 and is
 * removed on exit. Written only when the state changes.
 */
static char visible_file[PATH_MAX + 32];
static int published_visible = -1;

static void init_visible_file(void)
{
  const char *dir = getenv("XDG_RUNTIME_DIR");

  if (dir != NULL && dir[0] == '/')
    snprintf(visible_file, sizeof(visible_file), "%s/retrowebradio-visible", dir);
  else
    snprintf(visible_file, sizeof(visible_file), "/tmp/retrowebradio-%u-visible",
             (unsigned)getuid());
}

static void publish_visibility(void)
{
  Uint32 flags = SDL_GetWindowFlags(window);
  int visible = !(flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED));
  FILE *fp;

  if (visible == published_visible || visible_file[0] == '\0')
    return;
  if ((fp = fopen(visible_file, "w")) != NULL) {
    fprintf(fp, "%d\n", visible);
    fclose(fp);
    published_visible = visible;
  }
}

/* show (and raise), hide, or toggle the window on request of the tray */
static void handle_window_request(int req)
{
  Uint32 flags = SDL_GetWindowFlags(window);
  int hidden = (flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) != 0;

  if (req == WIN_SHOW || (req == WIN_TOGGLE && hidden)) {
    SDL_ShowWindow(window);
    SDL_RestoreWindow(window);
    SDL_RaiseWindow(window);
    refresh_now = 1;
  } else {
    SDL_HideWindow(window);
  }
}

/* ------------------------------------------------------------------ */
/* setup                                                               */
/* ------------------------------------------------------------------ */

/*
 * Locate a data file: explicit path, else next to the executable,
 * else current directory, else ~/.config/retrowebradio (a user's own
 * station list), else DATADIR (make install).
 */
static const char *find_datafile(const char *override, const char *name,
                                 char *buf, size_t size)
{
  char exe[PATH_MAX];
  ssize_t n;

  if (override != NULL)
    return override;

  n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
  if (n > 0) {
    exe[n] = '\0';
    snprintf(buf, size, "%s/%s", dirname(exe), name);
    if (access(buf, R_OK) == 0)
      return buf;
  }
  if (access(name, R_OK) == 0)
    return name;
  {
    const char *cfg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (cfg != NULL && cfg[0] == '/')
      snprintf(buf, size, "%s/retrowebradio/%s", cfg, name);
    else if (home != NULL)
      snprintf(buf, size, "%s/.config/retrowebradio/%s", home, name);
    else
      buf[0] = '\0';
    if (buf[0] != '\0' && access(buf, R_OK) == 0)
      return buf;
  }
  snprintf(buf, size, "%s/%s", DATADIR, name);
  return buf;
}

static void usage(const char *prog)
{
  fprintf(stderr,
          "usage: %s [-f] [-s stations.xml] [-F font.ttf] [-d seconds]\n"
          "  -f  fullscreen (scaled, aspect ratio kept)\n"
          "  -s  station list (default: next to the binary, ./,\n"
          "      ~/.config/retrowebradio/, " DATADIR ")\n"
          "  -F  TrueType font (default: VeraMono.ttf, searched like -s)\n"
          "  -d  startup delay in seconds (default 2, avoids starting\n"
          "      behind the taskbar during boot)\n", prog);
}

static void die(const char *what)
{
  fprintf(stderr, "%s: %s\n", what, SDL_GetError());
  exit(1);
}

int main(int argc, char *argv[])
{
  char stations_buf[PATH_MAX + 32], font_buf[PATH_MAX + 32];
  const char *stations_arg = NULL, *font_arg = NULL;
  const char *stations_path, *font_path;
  int fullscreen = 0, delay = 2, opt, running = 1;
  uint64_t next_redraw = 0, next_poll = 0, next_save = 0, t;
  int scrolling, presented;
  struct sigaction sa;

  while ((opt = getopt(argc, argv, "fs:F:d:h")) != -1) {
    switch (opt) {
    case 'f': fullscreen = 1; break;
    case 's': stations_arg = optarg; break;
    case 'F': font_arg = optarg; break;
    case 'd': delay = atoi(optarg); break;
    default: usage(argv[0]); return opt == 'h' ? 0 : 1;
    }
  }

  stations_path = find_datafile(stations_arg, STATIONSFILE, stations_buf, sizeof(stations_buf));
  font_path = find_datafile(font_arg, FONTFILE, font_buf, sizeof(font_buf));

  if (stations_load(stations_path, &stations) != 0)
    return 1;
  fprintf(stderr, "%d pages loaded from %s\n", stations.pages, stations_path);
  init_state_file();
  load_position();

  /* pkill/logout (SIGTERM) and Ctrl+C: leave the loop and save the dial */
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = on_signal;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);
  sa.sa_handler = on_window_signal;
  sigaction(SIGUSR1, &sa, NULL);
  sigaction(SIGUSR2, &sa, NULL);
  sigaction(SIGRTMIN, &sa, NULL);

  if (delay > 0)
    sleep((unsigned)delay);
  if (stop_requested)
    return 0;
  fprintf(stderr, "initializing...\n");

  /* window class / app id, matches StartupWMClass in retrowebradio.desktop,
     so panels and window lists show the radio icon and group the window */
  setenv("SDL_VIDEO_X11_WMCLASS", "retrowebradio", 0);
  setenv("SDL_VIDEO_WAYLAND_WMCLASS", "retrowebradio", 0);

  if (SDL_Init(SDL_INIT_VIDEO) != 0)
    die("SDL_Init");
  window = SDL_CreateWindow("RetroWebRadio", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                            WIN_WIDTH, WIN_HEIGHT,
                            fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
  if (window == NULL)
    die("SDL_CreateWindow");
  {
    SDL_Surface *icon = SDL_CreateRGBSurfaceWithFormatFrom(
        (void *)radio_icon_rgba, RADIO_ICON_W, RADIO_ICON_H, 32,
        RADIO_ICON_W * 4, SDL_PIXELFORMAT_RGBA32);
    if (icon != NULL) {
      SDL_SetWindowIcon(window, icon);  /* title bar, taskbar, alt-tab */
      SDL_FreeSurface(icon);
    }
  }
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
  if (renderer == NULL)
    renderer = SDL_CreateRenderer(window, -1, 0);
  if (renderer == NULL)
    die("SDL_CreateRenderer");
  SDL_RenderSetLogicalSize(renderer, WIN_WIDTH, WIN_HEIGHT);
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, WIN_WIDTH, WIN_HEIGHT);
  screen = SDL_CreateRGBSurfaceWithFormat(0, WIN_WIDTH, WIN_HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);
  background = SDL_CreateRGBSurfaceWithFormat(0, WIN_WIDTH, WIN_HEIGHT, 32, SDL_PIXELFORMAT_ARGB8888);
  target = screen;
  if (texture == NULL || screen == NULL || background == NULL)
    die("SDL canvas");
  SDL_ShowCursor(SDL_ENABLE);

  if (TTF_Init() != 0)
    die("TTF_Init");
  station_font = TTF_OpenFont(font_path, 22);
  station_font_big = TTF_OpenFont(font_path, 22);
  track_font = TTF_OpenFont(font_path, 15);
  if (station_font == NULL || station_font_big == NULL || track_font == NULL) {
    fprintf(stderr, "ERROR: cannot load font %s: %s\n", font_path, TTF_GetError());
    return 1;
  }

  fprintf(stderr, "started...\n");
  adopt_playing_station();
  init_visible_file();

  while (running && !stop_requested) {
    t = now_ms();
    if (window_request != WIN_NONE) {
      int req = window_request;
      window_request = WIN_NONE;
      handle_window_request(req);
    }
    running = process_events();
    publish_visibility();
    scan_step();
    update_tuning();
    play_current();
    reap_children();

    if (t >= next_poll) {
      get_current_track();
      next_poll = t + TRACK_POLL_MS;
    }
    scrolling = track_scrolls();
    presented = 0;
    if (refresh_now || search_dir != 0 || t >= next_redraw) {
      draw_everything(1);
      refresh_now = 0;
      next_redraw = t + IDLE_REDRAW_MS;
      presented = 1;
    } else if (scrolling) {
      draw_everything(0);
      presented = 1;
    }
    if (t >= next_save) {
      save_position();  /* writes only if the dial has moved */
      next_save = t + SAVE_DELAY_MS;
    }
    if (presented && scrolling) {
      /* smooth scrolling: ~60 fps. With vsync RenderPresent already
         waited for the next frame; without it, sleep the rest. */
      uint64_t el = now_ms() - t;
      if (el < 5)
        SDL_Delay((Uint32)(FRAME_MS - el));
    } else {
      SDL_Delay(TICK_MS);
    }
  }

  save_position();
  if (visible_file[0] != '\0')
    unlink(visible_file);

  if (tune_pid > 0)
    waitpid(tune_pid, NULL, 0);  /* let a pending tuning finish */
  stations_free(&stations);
  TTF_Quit();
  SDL_Quit();
  return 0;
}
