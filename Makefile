# RetroWebRadio
#
#   make                  build roehre and stations.xml
#   sudo make install     system-wide into PREFIX (default /usr/local),
#                         with entry in the application menu
#   sudo make uninstall   remove it again
#   make install-desktop  menu entry for the current user only, running
#                         roehre from BINDIR (default: this directory)
#   make install-pi       everything into RADIO_DIR (default
#                         /home/radio/radio), the Raspberry Pi layout
#   make -C radio         build the Raspberry Pi helpers (see radio/README.md)
#
# PREFIX is compiled into roehre (data directory); run 'make clean' after
# changing it.

CC       ?= gcc
PKGS      = sdl2 SDL2_ttf libxml-2.0
CFLAGS   ?= -O2 -g
# X11 SHAPE (optional): cuts the radio window to the outline of the case
XSHAPE   := $(shell pkg-config --exists x11 xext && echo yes)
ifeq ($(XSHAPE),yes)
PKGS     += x11 xext
CPPFLAGS += -DHAVE_XSHAPE
endif

CFLAGS   += -Wall -Wextra -std=c99 $(shell pkg-config --cflags $(PKGS))
LDLIBS   += $(shell pkg-config --libs $(PKGS)) -lm

PREFIX    ?= /usr/local
DATADIR    = $(PREFIX)/share/retrowebradio
CPPFLAGS  += -DDATADIR='"$(DATADIR)"'
RADIO_DIR ?= /home/radio/radio
STATIONS_PER_PAGE ?= 20

all: roehre stations.xml

roehre: roehre.o stations.o cabinet.o noise.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

roehre.o: roehre.c stations.h radio_icon.h cabinet.h noise.h
cabinet.o: cabinet.c cabinet.h logo.h
noise.o: noise.c noise.h
stations.o: stations.c stations.h

stations.xml: stationslist.txt stationslist2xml.sh
	./stationslist2xml.sh -n $(STATIONS_PER_PAGE) stationslist.txt > $@.tmp
	mv $@.tmp $@

install: roehre stations.xml
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(DATADIR)/icons \
	           $(DESTDIR)$(PREFIX)/share/applications \
	           $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps
	install -m 755 roehre retrowebradio-tray $(DESTDIR)$(PREFIX)/bin/
	install -m 644 VeraMono.ttf stations.xml $(DESTDIR)$(DATADIR)/
	install -m 644 icons/*.svg $(DESTDIR)$(DATADIR)/icons/
	install -d $(DESTDIR)$(DATADIR)/textures
	install -m 644 textures/*.bmp $(DESTDIR)$(DATADIR)/textures/
	install -m 644 icons/retrowebradio.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/
	sed 's|@BINDIR@|$(PREFIX)/bin|g' retrowebradio.desktop.in \
	    > $(DESTDIR)$(PREFIX)/share/applications/retrowebradio.desktop
	@[ -n "$(DESTDIR)" ] || ! command -v update-desktop-database >/dev/null || \
	    update-desktop-database -q $(PREFIX)/share/applications
	@[ -n "$(DESTDIR)" ] || ! command -v gtk-update-icon-cache >/dev/null || \
	    gtk-update-icon-cache -q -t $(PREFIX)/share/icons/hicolor || true

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/roehre $(DESTDIR)$(PREFIX)/bin/retrowebradio-tray \
	      $(DESTDIR)$(PREFIX)/share/applications/retrowebradio.desktop \
	      $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/retrowebradio.svg
	rm -rf $(DESTDIR)$(DATADIR)
	@[ -n "$(DESTDIR)" ] || ! command -v update-desktop-database >/dev/null || \
	    update-desktop-database -q $(PREFIX)/share/applications

install-pi: roehre stations.xml
	install -d $(DESTDIR)$(RADIO_DIR)
	install -m 755 roehre retrowebradio-tray $(DESTDIR)$(RADIO_DIR)/
	install -m 644 VeraMono.ttf stations.xml $(DESTDIR)$(RADIO_DIR)/
	install -d $(DESTDIR)$(RADIO_DIR)/icons
	install -m 644 icons/*.svg $(DESTDIR)$(RADIO_DIR)/icons/
	install -d $(DESTDIR)$(RADIO_DIR)/textures
	install -m 644 textures/*.bmp $(DESTDIR)$(RADIO_DIR)/textures/

# menu entry + icon for the current user, so the window list / taskbar
# shows the radio icon (matched via StartupWMClass=retrowebradio)
USER_PREFIX ?= $(HOME)/.local
BINDIR      ?= $(CURDIR)

install-desktop:
	install -d $(USER_PREFIX)/share/applications $(USER_PREFIX)/share/icons/hicolor/scalable/apps
	install -m 644 icons/retrowebradio.svg $(USER_PREFIX)/share/icons/hicolor/scalable/apps/
	sed 's|@BINDIR@|$(BINDIR)|g' retrowebradio.desktop.in > $(USER_PREFIX)/share/applications/retrowebradio.desktop
	@! command -v update-desktop-database >/dev/null || \
	    update-desktop-database -q $(USER_PREFIX)/share/applications

uninstall-desktop:
	rm -f $(USER_PREFIX)/share/applications/retrowebradio.desktop \
	      $(USER_PREFIX)/share/icons/hicolor/scalable/apps/retrowebradio.svg

clean:
	rm -f roehre *.o stations.xml.tmp

.PHONY: all install uninstall install-pi install-desktop uninstall-desktop clean
