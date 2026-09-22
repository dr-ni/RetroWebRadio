/*
 * RetroWebRadio - KY-040 rotary encoder daemons for the Raspberry Pi
 * (C) Niethammer, GNU GENERAL PUBLIC LICENSE Version 3
 *
 * One source for the three knobs; the Makefile builds
 *
 *   lpoti  (-DPOTI_L)  left knob:   volume -/+ via mpc,    push -> lpush
 *   mpoti  (-DPOTI_M)  middle knob: key Up/Down  (page),   push -> mpush
 *   rpoti  (-DPOTI_R)  right knob:  key Left/Right (tune), push -> rpush
 *
 * Keys are sent as fake X11 key events (XTest) to the focused window,
 * i.e. to roehre. GPIO access via wiringPi (default) or pigpio
 * (-DUSE_PIGPIO, needs root). Pins are BCM numbers for both backends.
 *
 * Pinout (BCM / physical pin / cable colour):
 *
 *   knob    DT            CLK           SW
 *   left    22 / P15 amb  27 / P13 grey 26 / P37 blue
 *   middle  24 / P18 yel  23 / P16 grn  25 / P22 orange
 *   right    6 / P31 grey 13 / P33 amb   5 / P29 white
 */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#ifdef USE_PIGPIO
#include <pigpio.h>
#else
#include <wiringPi.h>
#endif

#ifndef RADIO_DIR
#define RADIO_DIR "/home/radio/radio"
#endif

#if defined(POTI_L)
#define NAME   "left"
#define PIN_DT  22
#define PIN_CLK 27
#define PIN_SW  26
#define PUSH_CMD RADIO_DIR "/lpush"
#define USE_X11 0
#elif defined(POTI_M)
#define NAME   "middle"
#define PIN_DT  24
#define PIN_CLK 23
#define PIN_SW  25
#define PUSH_CMD RADIO_DIR "/mpush"
#define USE_X11 1
#define KEY_RIGHT  XK_Down
#define KEY_LEFT XK_Up
#elif defined(POTI_R)
#define NAME   "right"
#define PIN_DT   6
#define PIN_CLK 13
#define PIN_SW   5
#define PUSH_CMD RADIO_DIR "/rpush"
#define USE_X11 1
#define KEY_RIGHT  XK_Right
#define KEY_LEFT XK_Left
#else
#error "define one of POTI_L, POTI_M, POTI_R"
#endif

#if USE_X11
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
static Display *disp;

static void send_key(KeySym keysym)
{
  KeyCode keycode = XKeysymToKeycode(disp, keysym);
  if (keycode == 0)
    return;
  XTestGrabControl(disp, True);
  XTestFakeKeyEvent(disp, keycode, True, 0);
  XTestFakeKeyEvent(disp, keycode, False, 0);
  XSync(disp, False);
  XTestGrabControl(disp, False);
}

static void open_display(void)
{
  XInitThreads();  /* ISRs run in their own threads */
  if (getenv("DISPLAY") == NULL)
    setenv("DISPLAY", ":0", 1);
  /* during boot X may not be up yet: wait for it instead of crashing */
  while ((disp = XOpenDisplay(NULL)) == NULL) {
    fprintf(stderr, "waiting for X display %s...\n", getenv("DISPLAY"));
    sleep(2);
  }
}
#endif

#ifdef USE_PIGPIO
#define gpio_read(p) gpioRead(p)
#else
#define gpio_read(p) digitalRead(p)
#endif

enum { NONE, LEFT_1, LEFT_2, LEFT_3, RIGHT_1, RIGHT_2, RIGHT_3 };
static int state = NONE;
/* DT and CLK interrupts arrive in separate threads */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void turned(int right)
{
#if USE_X11
  send_key(right ? KEY_RIGHT : KEY_LEFT);
#else
  if (system(right ? "/usr/bin/mpc -q volume -2" : "/usr/bin/mpc -q volume +2") != 0)
    fprintf(stderr, "mpc volume failed\n");
#endif
}

/* quadrature state machine: DT/CLK 11 -> 10 -> 00 -> 01 = one step "right",
 * 11 -> 01 -> 00 -> 10 = one step "left" */
static void rotation(void)
{
  int dt, clk;

  pthread_mutex_lock(&lock);
  dt = gpio_read(PIN_DT);
  clk = gpio_read(PIN_CLK);

  if (dt && clk)
    state = NONE;
  else if (!dt && clk) {
    if (state == NONE)
      state = LEFT_1;
    else if (state == RIGHT_2) {
      state = RIGHT_3;
      turned(1);
    }
  } else if (dt && !clk) {
    if (state == NONE)
      state = RIGHT_1;
    else if (state == LEFT_2) {
      state = LEFT_3;
      turned(0);
    }
  } else {
    if (state == LEFT_1)
      state = LEFT_2;
    else if (state == RIGHT_1)
      state = RIGHT_2;
  }
  pthread_mutex_unlock(&lock);
}

static void push(void)
{
  if (!gpio_read(PIN_SW))
    usleep(5000);  /* debouncing */
  if (!gpio_read(PIN_SW)) {
    fprintf(stderr, NAME " button pushed\n");
    if (system(PUSH_CMD) != 0)  /* blocks this ISR thread: acts as debounce */
      fprintf(stderr, PUSH_CMD " failed\n");
  }
}

#ifdef USE_PIGPIO
static void isr_rot(int gpio, int level, uint32_t tick)
{
  (void)gpio; (void)level; (void)tick;
  rotation();
}

static void isr_push(int gpio, int level, uint32_t tick)
{
  (void)gpio; (void)level; (void)tick;
  push();
}

static int gpio_setup(void)
{
  if (gpioInitialise() < 0)
    return -1;
  gpioSetMode(PIN_DT, PI_INPUT);
  gpioSetMode(PIN_CLK, PI_INPUT);
  gpioSetMode(PIN_SW, PI_INPUT);
  gpioSetPullUpDown(PIN_SW, PI_PUD_UP);
  gpioSetISRFunc(PIN_DT, EITHER_EDGE, 0, isr_rot);
  gpioSetISRFunc(PIN_CLK, EITHER_EDGE, 0, isr_rot);
  gpioSetISRFunc(PIN_SW, FALLING_EDGE, 0, isr_push);
  return 0;
}
#else
static int gpio_setup(void)
{
  if (wiringPiSetupGpio() == -1)  /* BCM numbering */
    return -1;
  pinMode(PIN_DT, INPUT);
  pinMode(PIN_CLK, INPUT);
  pinMode(PIN_SW, INPUT);
  pullUpDnControl(PIN_SW, PUD_UP);
  wiringPiISR(PIN_DT, INT_EDGE_BOTH, rotation);
  wiringPiISR(PIN_CLK, INT_EDGE_BOTH, rotation);
  wiringPiISR(PIN_SW, INT_EDGE_FALLING, push);
  return 0;
}
#endif

int main(void)
{
#if USE_X11
  open_display();
#endif
  if (gpio_setup() < 0) {
    fprintf(stderr, "GPIO initialisation failed\n");
    return 1;
  }
  fprintf(stderr, NAME " knob started (DT=%d CLK=%d SW=%d, BCM)\n", PIN_DT, PIN_CLK, PIN_SW);
  for (;;)
    pause();  /* everything happens in the interrupt threads */
}
