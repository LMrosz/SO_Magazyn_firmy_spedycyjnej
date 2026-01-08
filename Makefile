CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lm

all: magazyn pracownicy ciezarowki dyspozytor generator

magazyn: magazyn.c utils.c utils.h
	$(CC) $(CFLAGS) -o magazyn magazyn.c utils.c $(LDFLAGS)

pracownicy: pracownicy.c utils.c utils.h
	$(CC) $(CFLAGS) -o pracownicy pracownicy.c utils.c $(LDFLAGS)

ciezarowki: ciezarowki.c utils.c utils.h
	$(CC) $(CFLAGS) -o ciezarowki ciezarowki.c utils.c $(LDFLAGS)

dyspozytor: dyspozytor.c utils.c utils.h
	$(CC) $(CFLAGS) -o dyspozytor dyspozytor.c utils.c $(LDFLAGS)

generator: generator.c utils.c utils.h
	$(CC) $(CFLAGS) -o generator generator.c utils.c $(LDFLAGS)

clean:
	rm -f magazyn pracownicy ciezarowki dyspozytor generator
	ipcrm -a 2>/dev/null || true

.PHONY: all clean