CC = gcc
CFLAGS = -O0 -g -I /usr/include/SDL -lSDL -lSDL_ttf
LIBS = `sdl-config --libs`  -lSDL_ttf
OBJECTS = roehre.o iniparser/iniparser.o

all: roehre

roehre: $(OBJECTS)
	$(CC) $(CFLAGS) -o roehre $(OBJECTS) iniparser/strlib.o iniparser/dictionary.o $(LIBS)

$(OBJECTS): $(HEADERS)

