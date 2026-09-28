# RetroWebRadio on a Raspberry Pi

This folder turns a Raspberry Pi with a small display, three KY-040 rotary
encoders and an audio output into a stand-alone web radio around `roehre`.

## Setup

On the Pi, as the radio user, in the repository:

```sh
sudo apt install mpd mpc libsdl2-dev libsdl2-ttf-dev libxml2-dev \
     libx11-dev libxext-dev pkg-config python3-gpiozero
make && make install-pi          # roehre and this folder to RADIO_DIR (/home/radio/radio)
make autostart                   # knobs, radio at login, mpd user service
```

`make autostart` (also `make -C radio autostart`):

- installs the knob daemon `potid.py` as systemd user service `potid`
  (`systemctl --user status potid`, `journalctl --user -u potid -f`),
- starts `roehre -f -d 2` at login via `~/.config/autostart`
  (other options: `make autostart GUI_ARGS="-f -g"`),
- enables the mpd user service if `~/.config/mpd/mpd.conf` exists,
- enables lingering, so the services also run without a login.

`make no-autostart` undoes it. Use the same `RADIO_DIR=...` everywhere if you
change it.

## Knobs

| Knob | Turn | Push (script) |
|------|------|---------------|
| left | volume (clockwise louder) | `lpush`: play/pause |
| middle | page (band) | `mpush`: switch audio output 1/2 |
| right | tune | `rpush`: start/stop the GUI |

`potid.py` handles all three knobs with gpiozero (all Pi models including the
Pi 5, no wiringPi/pigpio). Volume goes to mpc directly; tuning and page
changes go to roehre through its control FIFO
(`$XDG_RUNTIME_DIR/retrowebradio.ctl`), so the knobs work on X11 and
Wayland and while the radio window is hidden. Pins, wiring direction and the
action of each knob are in the table at the top of `potid.py`.
`./potid.py -v` prints every step. Any program can send commands to the FIFO,
e.g. `echo "scan +1" > $XDG_RUNTIME_DIR/retrowebradio.ctl` (see roehre.c).

### Wiring (BCM numbers)

| Knob | DT | CLK | SW |
|------|----|-----|----|
| left | GPIO22 (pin 15) | GPIO27 (pin 13) | GPIO26 (pin 37) |
| middle | GPIO24 (pin 18) | GPIO23 (pin 16) | GPIO25 (pin 22) |
| right | GPIO6 (pin 31) | GPIO13 (pin 33) | GPIO5 (pin 29) |

`+` of each KY-040 to 3.3 V (never 5 V), `GND` to ground. The internal
pull-ups are switched on, so modules without resistors work too. Middle and
right are wired with DT/CLK the other way round than left; `potid.py` and
`ky040_test.py` know that.

Testing: stop potid (`systemctl --user stop potid`), then
`./ky040_test.py [left|middle|right] [-v] [-r]` shows rest levels, steps and
button presses.

## Other files

| File | Purpose |
|------|---------|
| `ky040_test.py` | knob test (see above) |
| `startpoti.sh` | start potid.py by hand and playback (without the service) |
| `mpd.conf`, `asoundrc` | mpd configuration with the ALSA equalizer plugin (`libasound2-plugin-equal`) |
| `etc_raspotify_conf` | raspotify (Spotify Connect) configuration |
| `config.txt`, `cmdline.txt`, `splash-readme`, `asplashscreen`, `splash.png` | silent boot with a splash screen |
| `getcov.sh`, `getcovkiller.sh`, `master-*.png` | cover art for the current title via `sacad` and ImageMagick |
| `stations*.xml`, `stations.txt` | alternative station lists (`roehre -s ...`) |
| `check_correct_shutdown.sh` | show uptime history (`tuptime`), e.g. to verify clean shutdowns with a UPS HAT |
| `start-youtube-music` | YouTube Music in Chromium kiosk mode |
