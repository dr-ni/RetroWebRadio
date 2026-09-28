#!/usr/bin/env python3
"""
Simple test for the three KY-040 knobs of RetroWebRadio (gpiozero, works
on all Pi models incl. Pi 5). Stop lpoti/mpoti/rpoti first.

  python3 ky040_test.py            all three knobs
  python3 ky040_test.py left -v    one knob, also print every DT/CLK change
  python3 ky040_test.py -r         reverse the direction of all knobs
"""
import sys
from signal import pause
from gpiozero import Button, DigitalInputDevice

KNOBS = {             # BCM pins: DT, CLK, SW, reversed wiring
    "left":   (22, 27, 26, False),
    "middle": (24, 23, 25, True),     # DT/CLK the other way round
    "right":  (6, 13, 5, True),
}
verbose = "-v" in sys.argv
flip = "-r" in sys.argv
names = [a for a in sys.argv[1:] if a in KNOBS] or list(KNOBS)
# transition table: index prev<<2 | cur, state = DT<<1 | CLK
DELTA = [0, +1, -1, 0, -1, 0, 0, +1, +1, 0, 0, -1, 0, -1, +1, 0]
keep = []


def knob(name, dt_pin, clk_pin, sw_pin, rev):
    dt = DigitalInputDevice(dt_pin, pull_up=True)
    clk = DigitalInputDevice(clk_pin, pull_up=True)
    sw = Button(sw_pin, pull_up=True, bounce_time=0.02)
    st = {"prev": None, "count": 0}

    def changed():
        # pull_up=True: is_active means the pin is LOW, so invert
        cur = (0 if dt.is_active else 2) | (0 if clk.is_active else 1)
        if st["prev"] is None:
            st["prev"] = cur
        if cur == st["prev"]:
            return
        st["count"] += DELTA[st["prev"] << 2 | cur]
        if verbose:
            print(f"{name:6} DT={cur >> 1} CLK={cur & 1} count={st['count']}")
        # counting up = counter-clockwise (left knob's wiring), the others reversed
        if (st["count"] >= 4 and not rev) or (st["count"] <= -4 and rev):
            print(f"{name:6} <- step counter-clockwise")
            st["count"] = 0
        elif abs(st["count"]) >= 4:
            print(f"{name:6} -> step clockwise")
            st["count"] = 0
        elif cur == 3:
            st["count"] = 0          # back at rest without a full step
        st["prev"] = cur

    for pin in (dt, clk):
        pin.when_activated = changed
        pin.when_deactivated = changed
    sw.when_pressed = lambda: print(f"{name:6} button pressed")
    sw.when_released = lambda: print(f"{name:6} button released")
    changed()
    print(f"{name:6} ready (DT={dt_pin} CLK={clk_pin} SW={sw_pin}), "
          f"rest: DT={int(not dt.is_active)} CLK={int(not clk.is_active)} "
          f"SW={int(not sw.is_pressed)}")
    keep.extend((dt, clk, sw))


for n in names:
    dt_p, clk_p, sw_p, rev = KNOBS[n]
    knob(n, dt_p, clk_p, sw_p, rev != flip)
print("turn and press the knobs, Ctrl+C to quit")
try:
    pause()
except KeyboardInterrupt:
    pass
