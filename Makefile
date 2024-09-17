CC = gcc
CFLAGS = -Wall -I /usr/include/SDL -I /usr/include/libxml2 -lxml2 -lSDL -lSDL_ttf
LIBS = `sdl-config --libs`  -lSDL_ttf -lxml2

all: roehre

roehre: roehre.c xmlparse.c
	$(CC) $(CFLAGS) -o roehre roehre.c xmlparse.c $(LIBS)

clean:
	rm -f roehre
