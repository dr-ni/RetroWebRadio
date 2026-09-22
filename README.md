<img src="https://github.com/dr-ni/RetroWebRadio/blob/main/Niethammer-Audio.png" width=25% height=25%></br>
# RetroWebRadio
## A nice SDL Webradio GUI
[![onb](https://github.com/dr-ni/RetroWebRadio/blob/main/screen.png)](https://www.youtube.com/watch?v=MMwdwqVnOOw)

## Description
This is a simple mpd webradio SDL frontend which was initially launched by Florian Amrhein (http://amrhein.eu/nw/roehre/ui/).
It is a complete overwork of the initially made version in order to remove bugs and not requiring an external parser.
It shows the stations like the dial of an old tube radio; move the tuner over a station and after one second it starts playing.

The `radio/` folder contains everything needed to build the complete radio on a Raspberry Pi:
three KY-040 rotary encoder daemons (volume, page, tuning), helper scripts and boot/mpd configuration.
See [radio/README.md](radio/README.md).

## Requirements

- mpd, configured in /etc/mpd.conf, and mpc
- libsdl2-dev, libsdl2-ttf-dev, libxml2-dev, pkg-config

```sh
sudo apt install mpd mpc libsdl2-dev libsdl2-ttf-dev libxml2-dev pkg-config
```

## Build

```sh
make                 # builds roehre and stations.xml from stationslist.txt
make install         # optional: copies roehre, font and stations.xml to RADIO_DIR
                     # (default /home/radio/radio; make install RADIO_DIR=...)
```

## Stations

Edit `stationslist.txt`, one station per line:

```
[Deutschlandfunk] https://st01.sslstream.dlf.de/dlf/01/128/mp3/stream.mp3
```

Empty lines, lines starting with `#` and lines containing `§` are ignored.
`make` regenerates `stations.xml` (20 stations per page; `make STATIONS_PER_PAGE=16`),
or call `./stationslist2xml.sh [-n per_page] [list.txt] > stations.xml` directly.

## Usage

```
./roehre [-f] [-s stations.xml] [-F font.ttf] [-d seconds]
```

| Option | Meaning |
|--------|---------|
| `-f` | fullscreen, scaled to the display with the aspect ratio kept |
| `-s` | station list; default: next to the binary, then `./`, then `/usr/local/share/retrowebradio` |
| `-F` | TrueType font, searched like `-s` (default `VeraMono.ttf`) |
| `-d` | startup delay in seconds (default 2, avoids starting behind the taskbar at boot) |

| Key | Action |
|-----|--------|
| Left / Right | move the tuner (wraps to the previous/next page) |
| Up / Down | next / previous page |
| r / l | scan to the next station right / left |
| v, +, keypad +, volume-up key | volume +3 |
| -, keypad -, volume-down key | volume -3 |
| m, mute key | mute / unmute (restores the previous volume) |
| mouse click / touch | put the tuner there |
| q, Esc | quit |

The dial position (page and tuner) is saved in `~/.local/state/retrowebradio/position`
and restored on the next start, so the radio comes back on the last station.

## Tray icon

`retrowebradio-tray` puts a radio icon into the panel's system tray (Gtk.StatusIcon).
For panels that only show AppIndicators (e.g. GNOME with the AppIndicator extension)
start it with `RETROWEBRADIO_TRAY=indicator`.

- menu: current title, show/hide the radio, play/pause, louder, quieter, mute
- middle click: mute on/off, scroll wheel: volume
- "Quit" closes the radio GUI, stops playback (`mpc stop`) and ends the tray
- menu in German when the locale is German, English otherwise
- the icon is crossed out when muted and grey when mpd is not reachable; the tooltip
  shows volume and title, updated immediately via `mpc idleloop`

```sh
sudo apt install python3-gi gir1.2-ayatanaappindicator3-0.1
./retrowebradio-tray &
```

It starts `roehre` from its own directory (or from `$PATH`). For autostart, add
`/path/to/retrowebradio-tray` to the desktop session's startup applications.
