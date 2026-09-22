/*
 * sendkey - send one fake X11 key press to the focused window (XTest)
 *
 * usage: sendkey KEYSYM [KEYSYM...]     e.g.  sendkey Left
 *
 * Replaces the former keyleft/Keyleft/... helpers (whose names only
 * differed in case and collided on case-insensitive file systems):
 *
 *   keyleft  -> sendkey Left        Keyleft  -> sendkey l   (scan left)
 *   keyright -> sendkey Right       Keyright -> sendkey r   (scan right)
 *   keyup    -> sendkey Up          Keyplus  -> sendkey v   (volume +)
 *   keydown  -> sendkey Down        Keyminus -> sendkey KP_Subtract (volume -)
 *
 * (C) Niethammer, GNU GENERAL PUBLIC LICENSE Version 3
 */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

int main(int argc, char *argv[])
{
  Display *disp;
  int i, ret = 0;

  if (argc < 2) {
    fprintf(stderr, "usage: %s KEYSYM [KEYSYM...]   (e.g. Left, Right, Up, Down, l, r, v)\n", argv[0]);
    return 2;
  }
  if (getenv("DISPLAY") == NULL)
    setenv("DISPLAY", ":0", 1);
  disp = XOpenDisplay(NULL);
  if (disp == NULL) {
    fprintf(stderr, "%s: cannot open display %s\n", argv[0], getenv("DISPLAY"));
    return 1;
  }
  for (i = 1; i < argc; i++) {
    KeySym sym = XStringToKeysym(argv[i]);
    KeyCode code = sym != NoSymbol ? XKeysymToKeycode(disp, sym) : 0;

    if (code == 0) {
      fprintf(stderr, "%s: unknown key '%s'\n", argv[0], argv[i]);
      ret = 1;
      continue;
    }
    XTestGrabControl(disp, True);
    XTestFakeKeyEvent(disp, code, True, 0);
    XTestFakeKeyEvent(disp, code, False, 0);
    XSync(disp, False);
    XTestGrabControl(disp, False);
  }
  XCloseDisplay(disp);
  return ret;
}
