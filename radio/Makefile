# Raspberry Pi part of RetroWebRadio, see README.md
#
#   make install        potid.py, scripts and templates to RADIO_DIR
#   make autostart      knobs as systemd user service, radio GUI at login,
#                       mpd user service (if ~/.config/mpd/mpd.conf exists)
#   make no-autostart   undo "make autostart"
#
# Run as the radio user, without sudo. The top-level "make install-pi"
# installs roehre and this part together.

RADIO_DIR ?= /home/radio/radio
GUI_ARGS  ?= -f -d 2
SCRIPTS    = lpush mpush rpush startpoti.sh potid.py ky040_test.py
UNIT_DIR   = $(HOME)/.config/systemd/user
XDG_AUTO   = $(HOME)/.config/autostart

all:
	@echo "nothing to build; see: make install, make autostart"

install:
	install -d $(DESTDIR)$(RADIO_DIR)
	install -m 755 $(SCRIPTS) $(DESTDIR)$(RADIO_DIR)/

autostart: install
	install -d $(UNIT_DIR) $(XDG_AUTO)
	sed 's|@RADIO_DIR@|$(RADIO_DIR)|g' potid.service.in > $(UNIT_DIR)/potid.service
	sed -e 's|@RADIO_DIR@|$(RADIO_DIR)|g' -e 's|@GUI_ARGS@|$(GUI_ARGS)|g' \
	    retrowebradio-autostart.desktop.in > $(XDG_AUTO)/retrowebradio.desktop
	systemctl --user daemon-reload
	systemctl --user enable --now potid
	@if [ -f $(HOME)/.config/mpd/mpd.conf ]; then systemctl --user enable --now mpd; fi
	-loginctl enable-linger $(USER)
	@echo "autostart: knobs running (systemctl --user status potid), radio starts at next login"

no-autostart:
	-systemctl --user disable --now potid
	rm -f $(UNIT_DIR)/potid.service $(XDG_AUTO)/retrowebradio.desktop
	-systemctl --user daemon-reload

.PHONY: all install autostart no-autostart
