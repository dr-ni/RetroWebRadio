#!/bin/bash
# tell MPD to stop playing anything
mpc clear
# init USB port
/bin/stty -F /dev/ttyACM0 9600
export SDL_NOMOUSE=1
export SDL_NO_RAWKBD=1
cd /home/florian/projekte/roehre
# start UI and restart if it crashes
while true; do
  /bin/stty -F /dev/ttyACM0 9600
  sleep 1
  ./roehre >> /tmp/roehrelog
  sleep 5
done
