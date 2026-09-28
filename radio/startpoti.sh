#!/bin/bash
# start the knob daemon by hand (normally the potid user service does it) and playback
RADIO_DIR=${RADIO_DIR:-/home/radio/radio}

if systemctl --user is-active --quiet potid 2>/dev/null || pgrep -f "$RADIO_DIR/potid.py" >/dev/null; then
  echo "knobs already running"
else
  "$RADIO_DIR/potid.py" > /tmp/potid.log 2>&1 &
  echo "knobs started"
fi

mpc enable only 1 > /dev/null 2>&1
mpc volume 50 > /dev/null 2>&1
mpc play > /dev/null 2>&1 &
echo "radio started"
