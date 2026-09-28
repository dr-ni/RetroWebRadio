#!/usr/bin/env python3
"""
potid.py - one daemon for the three KY-040 knobs of RetroWebRadio.

No wiringPi/pigpio, no X11 key events.
Uses gpiozero (preinstalled on Raspberry Pi OS, all models incl. Pi 5)
and talks to roehre through its control FIFO, so the knobs also work
under Wayland and while the radio window is hidden.

  potid.py          run (normally as systemd user service: make autostart)
  potid.py -v       also print every step and button press
"""
import os
import subprocess
import sys
import threading
from signal import pause

from gpiozero import Button, DigitalInputDevice

HERE = os.path.dirname(os.path.realpath(__file__))
VERBOSE = "-v" in sys.argv

# BCM pins DT, CLK, SW and whether DT/CLK are wired the other way round
# (then counting up means clockwise). Actions for clockwise, counter-
# clockwise and push: ("mpc", args...), ("ctl", command) or ("script", name).
KNOBS = {
    "left": dict(pins=(22, 27, 26), reversed=False,
                 cw=("mpc", "volume", "+2"), ccw=("mpc", "volume", "-2"),
                 push=("script", "lpush")),
    "middle": dict(pins=(24, 23, 25), reversed=True,
                   cw=("ctl", "page -1"), ccw=("ctl", "page +1"),
                   push=("ctl", "cover")),       # ("script", "mpush"): switch output
    "right": dict(pins=(6, 13, 5), reversed=True,
                  cw=("ctl", "tune +1"), ccw=("ctl", "tune -1"),
                  push=("script", "rpush")),
}
STEPS_PER_DETENT = 4          # 2 for encoders that also rest at DT=CLK=0

# transition table: index prev<<2 | cur, state = DT<<1 | CLK
DELTA = [0, +1, -1, 0, -1, 0, 0, +1, +1, 0, 0, -1, 0, -1, +1, 0]


def ctl_path():
    run = os.environ.get("XDG_RUNTIME_DIR") or "/run/user/%d" % os.getuid()
    path = os.path.join(run, "retrowebradio.ctl")
    return path if os.path.exists(path) else "/tmp/retrowebradio-%d.ctl" % os.getuid()


def run(action, name):
    kind, *args = action
    if VERBOSE:
        print(f"{name:6} {kind} {' '.join(args)}", flush=True)
    try:
        if kind == "mpc":
            subprocess.Popen(["mpc", "-q", *args], stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL)
        elif kind == "ctl":
            # non-blocking: if roehre does not run there is no reader
            fd = os.open(ctl_path(), os.O_WRONLY | os.O_NONBLOCK)
            try:
                os.write(fd, (args[0] + "\n").encode())
            finally:
                os.close(fd)
        elif kind == "script":
            script = os.path.join(HERE, args[0])
            if os.access(script, os.X_OK):
                subprocess.Popen([script], stdout=subprocess.DEVNULL,
                                 stderr=subprocess.DEVNULL, start_new_session=True)
            elif args[0] == "lpush":
                subprocess.Popen(["mpc", "-q", "toggle"])
            elif args[0] == "rpush":
                run(("ctl", "toggle"), name)
    except OSError as e:
        if VERBOSE:
            print(f"{name:6} {kind} failed: {e}", flush=True)


class Knob:
    def __init__(self, name, cfg):
        dt_pin, clk_pin, sw_pin = cfg["pins"]
        self.name, self.cfg = name, cfg
        self.lock = threading.Lock()
        self.prev, self.count = None, 0
        self.dt = DigitalInputDevice(dt_pin, pull_up=True)
        self.clk = DigitalInputDevice(clk_pin, pull_up=True)
        self.sw = Button(sw_pin, pull_up=True, bounce_time=0.05)
        for pin in (self.dt, self.clk):
            pin.when_activated = self.changed
            pin.when_deactivated = self.changed
        self.sw.when_pressed = lambda: run(cfg["push"], name)
        self.changed()

    def changed(self):
        with self.lock:
            # pull-up: is_active means LOW, so invert to get the level
            cur = (0 if self.dt.is_active else 2) | (0 if self.clk.is_active else 1)
            if self.prev is None or cur == self.prev:
                self.prev = cur
                return
            self.count += DELTA[self.prev << 2 | cur]
            self.prev = cur
            if abs(self.count) >= STEPS_PER_DETENT:
                up = self.count > 0
                self.count = 0
                clockwise = up == self.cfg["reversed"]
                run(self.cfg["cw"] if clockwise else self.cfg["ccw"], self.name)
            elif cur == 3 and STEPS_PER_DETENT == 4:
                self.count = 0          # back at rest without a full step


def main():
    knobs = [Knob(n, c) for n, c in KNOBS.items()]
    print("potid: %d knobs ready (control FIFO %s)" % (len(knobs), ctl_path()), flush=True)
    pause()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
