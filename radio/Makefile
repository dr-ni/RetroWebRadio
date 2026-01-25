CC = gcc
CFLAGS = -Wall -I /usr/include/SDL -I /usr/include/libxml2 -lxml2 -lSDL -lSDL_ttf
LIBS = `sdl-config --libs`  -lSDL_ttf -lxml2

all: roehre ctl

roehre: roehre.c xmlparse.c
	$(CC) $(CFLAGS) -o roehre roehre.c xmlparse.c $(LIBS)

ctl:
	$(CC) -Wall keyright.c -lX11 -lXtst -o keyright
	$(CC) -Wall Keyright.c -lX11 -lXtst -o Keyright
	$(CC) -Wall Keyleft.c -lX11 -lXtst -o Keyleft
	$(CC) -Wall keyleft.c -lX11 -lXtst -o keyleft
	$(CC) -Wall keyup.c -lX11 -lXtst -o keyup
	$(CC) -Wall keydown.c -lX11 -lXtst -o keydown
	$(CC) -Wall Keyplus.c -lX11 -lXtst -o Keyplus
	$(CC) -Wall Keyminus.c -lX11 -lXtst -o Keyminus
	$(CC) -Wall -g lpoti.c -l wiringPi -o lpoti
	$(CC) -Wall -g mpoti.c -l wiringPi -lX11 -lXtst -o mpoti
	$(CC) -Wall -g rpoti.c -l wiringPi -lX11 -lXtst -o rpoti
	
$(OBJECTS): $(HEADERS)

clean:keyup
	rm -f roehre keyright Keyright keyleft Keyleft keyup keydown Keyplus Keyminus lpoti mpoti rpoti
