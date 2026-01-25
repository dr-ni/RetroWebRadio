#!/bin/sh
while true
do
#  mpc current > current_song.txt
  id=$(pgrep getcov )
  echo "pid=$id"
  kill "$id"
  /home/radio/radio/getcov.sh > /dev/null 2>&1 &
  mpc idle player
done

