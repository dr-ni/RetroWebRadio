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
| v, +, keypad + | volume +3 |
| -, keypad - | volume -3 |
| mouse click / touch | put the tuner there |
| q, Esc | quit |
