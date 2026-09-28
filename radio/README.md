# RetroWebRadio on a Raspberry Pi

This folder turns a Raspberry Pi with a small display, three KY-040 rotary
encoders and an audio output into a stand-alone web radio around `roehre`.

## Knobs

| Knob | Turn | Push (script) | Daemon |
|------|------|---------------|--------|
| left | volume -/+ (mpc) | `lpush`: play/pause | `lpoti` |
| middle | page up/down | `mpush`: switch audio output 1/2 | `mpoti` |
| right | tune left/right | `rpush`: start/stop the GUI | `rpoti` |

`mpoti` and `rpoti` send fake X11 key presses (XTest) to the focused
window, i.e. to `roehre`. The GUI therefore has to run on X11 (on a
Wayland desktop such as Raspberry Pi OS Bookworm's default, switch to X11
with `raspi-config` → Advanced → Wayland, or run roehre via XWayland with
`SDL_VIDEODRIVER=x11`).

### Wiring (BCM numbers)

| Knob | DT | CLK | SW |
|------|----|-----|----|
| left | GPIO22 (pin 15) | GPIO27 (pin 13) | GPIO26 (pin 37) |
| middle | GPIO24 (pin 18) | GPIO23 (pin 16) | GPIO25 (pin 22) |
| right | GPIO6 (pin 31) | GPIO13 (pin 33) | GPIO5 (pin 29) |

`+` of each KY-040 to 3.3 V (never 5 V), `GND` to ground. All three pins
use the Pi's internal pull-ups as well, so modules without resistors work.

Testing a knob: run it in the foreground with `-v`, e.g. `./lpoti -v`; every
DT/CLK change and each detected step is printed. If only every second detent
is counted (encoders that also rest at DT=CLK=0), start it with `-s 2`.
If a knob turns the wrong way, start it with `-r` (or swap DT and CLK).
Without the daemons, `gpiomon -c gpiochip0 -e both --bias pull-up 22 27 26`
(package gpiod) shows the raw edges.

## potid.py: one daemon for all knobs (recommended)

`potid.py` handles all three knobs in Python with gpiozero (preinstalled on
Raspberry Pi OS, all models including the Pi 5). It needs neither wiringPi nor
pigpio and no X11 key events: tuning and page changes go to roehre through its
control FIFO (`$XDG_RUNTIME_DIR/retrowebradio.ctl`), so the knobs work under
Wayland and while the radio window is hidden. Volume goes to mpc directly.

```sh
make install-py                   # potid.py and scripts to RADIO_DIR, no wiringPi needed
./potid.py -v                     # test in the foreground
cp potid.service ~/.config/systemd/user/ && systemctl --user enable --now potid
```

Pins, wiring direction and the action of each knob are in the table at the
top of `potid.py`. `startpoti.sh` starts potid.py; `POTI=c startpoti.sh` starts
the C daemons below instead. Any program can send commands to the FIFO too,
e.g. `echo "scan +1" > $XDG_RUNTIME_DIR/retrowebradio.ctl` (see roehre.c).

## Build and install

```sh
sudo apt install libx11-dev libxtst-dev
# plus wiringPi (.deb from github.com/WiringPi/WiringPi/releases)
# or pigpio (sudo apt install pigpio; not supported on the Pi 5)
make                    # sendkey, lpoti, mpoti, rpoti (wiringPi)
make PIGPIO=1           # same with pigpio (daemons must run as root)
make install            # to RADIO_DIR, default /home/radio/radio
make -C .. install-pi   # roehre, font and stations.xml to the same place
```

Use the same `RADIO_DIR=...` for both installs if you change it; the
daemons find `lpush`/`mpush`/`rpush` there, and the scripts honour the
`RADIO_DIR` environment variable.

`startpoti.sh` starts the three daemons and playback; call it from the
desktop autostart together with `roehre -f`.

## Other files

| File | Purpose |
|------|---------|
| `ky040_test.py` | test the knobs: `./ky040_test.py [left|middle|right] [-v] [-r]` shows rest levels, steps and button presses |
| `sendkey` | send a key to the GUI from scripts, e.g. `sendkey l` (scan left), `sendkey Right`, `sendkey v` |
| `mpd.conf`, `asoundrc` | mpd configuration with the ALSA equalizer plugin (`libasound2-plugin-equal`) |
| `etc_raspotify_conf` | raspotify (Spotify Connect) configuration |
| `config.txt`, `cmdline.txt`, `splash-readme`, `asplashscreen`, `splash.png` | silent boot with a splash screen |
| `getcov.sh`, `getcovkiller.sh`, `master-*.png` | cover art for the current title via `sacad` and ImageMagick |
| `stations*.xml`, `stations.txt` | alternative station lists (`roehre -s ...`) |
| `check_correct_shutdown.sh` | show uptime history (`tuptime`), e.g. to verify clean shutdowns with a UPS HAT |
| `start-youtube-music` | YouTube Music in Chromium kiosk mode |
