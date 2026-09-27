#!/bin/bash
# start the knob daemon(s) and playback
#   potid.py (default): one Python daemon for all knobs, works on X11 and Wayland
#   POTI=c startpoti.sh: the old C daemons lpoti/mpoti/rpoti (wiringPi, X11)
RADIO_DIR=${RADIO_DIR:-/home/radio/radio}

if [ "${POTI:-py}" = c ]; then
  for p in lpoti mpoti rpoti; do
    pgrep -x "$p" >/dev/null && continue          # never twice
    "$RADIO_DIR/$p" > "/tmp/$p.log" 2>&1 &
  done
else
  pgrep -f "$RADIO_DIR/potid.py" >/dev/null ||
    "$RADIO_DIR/potid.py" > /tmp/potid.log 2>&1 &
fi
echo "knobs started"

mpc enable only 1 > /dev/null 2>&1
mpc volume 50 > /dev/null 2>&1
mpc play > /dev/null 2>&1 &
echo "radio started"
