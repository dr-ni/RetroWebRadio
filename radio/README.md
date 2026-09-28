# RetroWebRadio on a Raspberry Pi

This folder turns a Raspberry Pi with a display, three KY-040 rotary encoders and an
audio output (e.g. a HiFiBerry DAC+) into a stand-alone web radio around `roehre`.
Meant for Raspberry Pi OS Trixie (X11 or Wayland); older versions work too.

## Setup

On the Pi, as the radio user, in the repository:

```sh
sudo apt install mpd mpc libsdl2-dev libsdl2-ttf-dev libxml2-dev pkg-config \
     libx11-dev libxext-dev python3-gpiozero python3-pil \
     libasound2-plugin-equal pipewire-alsa
sudo systemctl disable --now mpd.service mpd.socket   # mpd runs as user service
make && make install-pi          # roehre and this folder to RADIO_DIR (/home/radio/radio)
make autostart                   # knobs, radio at login, mpd
wpctl status                     # ID of the sound card under "Sinks"
wpctl set-default <ID>
```

`make autostart` (also `make -C radio autostart`), without sudo:

- installs the knob daemon `potid.py` as systemd user service `potid`
  (`systemctl --user status potid`, `journalctl --user -u potid -f`),
- starts `roehre -f -d 2` at login via `~/.config/autostart`
  (other options: `make autostart GUI_ARGS="-f -g"`),
- installs `mpd.conf` to `~/.config/mpd/` and `asoundrc` to `~/.asoundrc` unless they
  exist, and enables the mpd user service,
- enables lingering, so the services also run without a login.

`make no-autostart` undoes it (the mpd configuration stays). Use the same
`RADIO_DIR=...` everywhere if you change it.

## Audio

mpd runs as user service of the radio user and plays through PipeWire, which also
carries roehre's tuning static; so both share the sound card, and there is no "Device or
resource busy". Output 1 ("internal") goes through the ALSA equalizer plugin in front of
PipeWire (`asoundrc`):

```sh
alsamixer -D equal            # 10-band equalizer, saved automatically
mpc outputs                   # 1 internal (equalizer), 2 external (PipeWire directly)
```

`mpush` switches between output 1 and 2; set `target` in `mpd.conf` to send output 2 to
another sink (e.g. HDMI).

## Knobs

| Knob | Turn | Push |
|------|------|------|
| left | volume (clockwise louder) | `lpush`: play/pause |
| middle | page (band) | cover view on/off |
| right | tune | `rpush`: start/stop the radio |

`potid.py` handles all three knobs with gpiozero (all Pi models including the Pi 5, no
wiringPi/pigpio). Volume goes to mpc directly; tuning, page and cover go to roehre through
its control FIFO (`$XDG_RUNTIME_DIR/retrowebradio.ctl`), so the knobs work on X11 and
Wayland and while the radio window is hidden. Pins, wiring direction and the action of
each knob are in the table at the top of `potid.py` (e.g. `("script", "mpush")` on the
middle button to switch the audio output instead). `./potid.py -v` prints every step.

### Wiring (BCM numbers)

| Knob | DT | CLK | SW |
|------|----|-----|----|
| left | GPIO22 (pin 15) | GPIO27 (pin 13) | GPIO26 (pin 37) |
| middle | GPIO24 (pin 18) | GPIO23 (pin 16) | GPIO25 (pin 22) |
| right | GPIO6 (pin 31) | GPIO13 (pin 33) | GPIO5 (pin 29) |

`+` of each KY-040 to 3.3 V (never 5 V), `GND` to ground. The internal pull-ups are
switched on, so modules without resistors work too. Middle and right are wired with
DT/CLK the other way round than left; `potid.py` and `ky040_test.py` know that.

### Testing the knobs

```sh
systemctl --user stop potid                        # frees the pins
./ky040_test.py [left|middle|right] [-v] [-r]      # rest levels, steps, button presses
systemctl --user start potid
```

In rest DT, CLK and SW must read 1. "GPIO busy" means another process holds the pins
(`gpioinfo` shows which). If a knob counts only every second detent, set
`STEPS_PER_DETENT = 2` in `potid.py`; if it turns the wrong way, flip its `reversed`.

## Other files

| File | Purpose |
|------|---------|
| `mpd.conf`, `asoundrc` | mpd user service configuration and equalizer (see Audio) |
| `lpush`, `mpush`, `rpush` | knob button actions: play/pause, switch output, start/stop the radio |
| `startpoti.sh` | start potid.py by hand and playback (without the service) |
| `ky040_test.py` | knob test (see above) |
| `potid.service.in`, `retrowebradio-autostart.desktop.in` | templates for `make autostart` |
| `config.txt`, `cmdline.txt`, `splash-readme`, `asplashscreen`, `splash.png`, `splash.mov` | silent boot with a splash screen; take over single lines into `/boot/firmware/config.txt` rather than copying the file |
| `etc_raspotify_conf` | raspotify (Spotify Connect) configuration |
| `stations*.xml`, `stations.txt` | alternative station lists (`roehre -s ...`) |
| `check_correct_shutdown.sh` | show uptime history (`tuptime`), e.g. to verify clean shutdowns with a UPS HAT |
| `start-youtube-music` | YouTube Music in Chromium kiosk mode |
