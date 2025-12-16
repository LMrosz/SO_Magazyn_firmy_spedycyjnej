CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lm

all: magazyn pracownicy ciezarowki

magazyn: magazyn.c utils.c utils.h
	$(CC) $(CFLAGS) -o magazyn magazyn.c utils.c $(LDFLAGS)

pracownicy: pracownicy.c utils.c utils.h
	$(CC) $(CFLAGS) -o pracownicy pracownicy.c utils.c $(LDFLAGS)

ciezarowki: ciezarowki.c utils.c utils.h
	$(CC) $(CFLAGS) -o ciezarowki ciezarowki.c utils.c $(LDFLAGS)

clean:
	rm -f magazyn pracownicy ciezarowki
	ipcrm -a 2>/dev/null || true

.PHONY: all clean
