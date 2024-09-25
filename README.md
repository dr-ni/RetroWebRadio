# RetroWebRadio
A nice SDL Webradio GUI

![onb](https://github.com/dr-ni/RetroWebRadio/blob/main/screen.png)

## Description
This is a simple mpd webradio SDL frontend which was initially launched by Florian Arnhein (http://amrhein.eu/nw/roehre/ui/).
It is a complete overwork of the initially made version in order to remove bugs and not requiring an external parser.
This version has now also included 3 potentiometer-daemons for gpio control with rotary encoders KY-040 on a Raspberry PI.
The radio stations are now predefined in a stationslist.txt and has to be converted to a stations.xml inifile before usage.

## Requirements

- mpd must be installed and configured in /etc/mpd.conf.
- mpc must be installed.
- libxml2-dev and libsdl-ttf2.0-dev must be installed.

## Development

Build:
```sh
make
```

## Usage
```
./stationslist2xml.sh
./roehre
```

