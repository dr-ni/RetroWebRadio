#!/bin/bash
# start the three knob daemons (restart once if a start fails) and the radio
RADIO_DIR=${RADIO_DIR:-/home/radio/radio}

for p in lpoti mpoti rpoti; do
  "$RADIO_DIR/$p" > "/tmp/$p.log" 2>&1 &
  sleep 1
  if pgrep -x "$p" >/dev/null; then
    echo "$p running"
  else
    echo "$p stopped, retrying"
    echo "retry" >> "/tmp/$p.log"
    pkill -x "$p"
    "$RADIO_DIR/$p" >> "/tmp/$p.log" 2>&1 &
  fi
done
echo "potis started"

mpc enable only 1 > /dev/null 2>&1
mpc volume 50 > /dev/null 2>&1
mpc play > /dev/null 2>&1 &
echo "radio started"
