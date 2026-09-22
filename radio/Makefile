# Raspberry Pi helpers for RetroWebRadio, see README.md
#
#   make              sendkey + lpoti/mpoti/rpoti (wiringPi)
#   make PIGPIO=1     same, but the knob daemons use pigpio (run as root)
#   make mpdevent     optional song monitor (libmpd)
#   make install      copy binaries and scripts to RADIO_DIR

CC        ?= gcc
CFLAGS    ?= -O2 -g
CFLAGS    += -Wall -Wextra
RADIO_DIR ?= /home/radio/radio

ifeq ($(PIGPIO),1)
GPIO_DEF  = -DUSE_PIGPIO
GPIO_LIBS = -lpigpio -lrt
else
GPIO_LIBS = -lwiringPi
endif

POTIS   = lpoti mpoti rpoti
SCRIPTS = lpush mpush rpush startpoti.sh

all: sendkey $(POTIS)

sendkey: sendkey.c
	$(CC) $(CFLAGS) -o $@ $< -lX11 -lXtst

lpoti: poti.c
	$(CC) $(CFLAGS) $(GPIO_DEF) -DPOTI_L -DRADIO_DIR='"$(RADIO_DIR)"' -o $@ $< $(GPIO_LIBS) -lpthread
mpoti: poti.c
	$(CC) $(CFLAGS) $(GPIO_DEF) -DPOTI_M -DRADIO_DIR='"$(RADIO_DIR)"' -o $@ $< $(GPIO_LIBS) -lpthread -lX11 -lXtst
rpoti: poti.c
	$(CC) $(CFLAGS) $(GPIO_DEF) -DPOTI_R -DRADIO_DIR='"$(RADIO_DIR)"' -o $@ $< $(GPIO_LIBS) -lpthread -lX11 -lXtst

mpdevent: mpdevent.c
	$(CC) $(CFLAGS) -o $@ $< -I/usr/include/libmpd-1.0/libmpd -lmpd

install: all
	install -d $(DESTDIR)$(RADIO_DIR)
	install -m 755 sendkey $(POTIS) $(SCRIPTS) $(DESTDIR)$(RADIO_DIR)/

clean:
	rm -f sendkey $(POTIS) mpdevent

.PHONY: all install clean
