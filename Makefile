# RetroWebRadio
#
#   make                 build roehre and stations.xml
#   make install         install into $(RADIO_DIR) (default /home/radio/radio)
#   make -C radio        build the Raspberry Pi helpers (see radio/README.md)

CC       ?= gcc
PKGS      = sdl2 SDL2_ttf libxml-2.0
CFLAGS   ?= -O2 -g
CFLAGS   += -Wall -Wextra -std=c99 $(shell pkg-config --cflags $(PKGS))
LDLIBS   += $(shell pkg-config --libs $(PKGS))

RADIO_DIR ?= /home/radio/radio
STATIONS_PER_PAGE ?= 20

all: roehre stations.xml

roehre: roehre.o stations.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

roehre.o: roehre.c stations.h radio_icon.h
stations.o: stations.c stations.h

stations.xml: stationslist.txt stationslist2xml.sh
	./stationslist2xml.sh -n $(STATIONS_PER_PAGE) stationslist.txt > $@.tmp
	mv $@.tmp $@

install: roehre stations.xml
	install -d $(DESTDIR)$(RADIO_DIR)
	install -m 755 roehre retrowebradio-tray $(DESTDIR)$(RADIO_DIR)/
	install -m 644 VeraMono.ttf stations.xml $(DESTDIR)$(RADIO_DIR)/
	install -d $(DESTDIR)$(RADIO_DIR)/icons
	install -m 644 icons/*.svg $(DESTDIR)$(RADIO_DIR)/icons/

# menu entry + icon for the current user, so the window list / taskbar
# shows the radio icon (matched via StartupWMClass=retrowebradio)
USER_PREFIX ?= $(HOME)/.local
BINDIR      ?= $(CURDIR)

install-desktop:
	install -d $(USER_PREFIX)/share/applications $(USER_PREFIX)/share/icons/hicolor/scalable/apps
	install -m 644 icons/retrowebradio.svg $(USER_PREFIX)/share/icons/hicolor/scalable/apps/
	sed 's|@BINDIR@|$(BINDIR)|g' retrowebradio.desktop.in > $(USER_PREFIX)/share/applications/retrowebradio.desktop
	-update-desktop-database -q $(USER_PREFIX)/share/applications 2>/dev/null

uninstall-desktop:
	rm -f $(USER_PREFIX)/share/applications/retrowebradio.desktop \
	      $(USER_PREFIX)/share/icons/hicolor/scalable/apps/retrowebradio.svg

clean:
	rm -f roehre *.o stations.xml.tmp

.PHONY: all install install-desktop uninstall-desktop clean
