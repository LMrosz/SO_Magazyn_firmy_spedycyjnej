CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lm

TARGETS = magazyn pracownicy pracownik4 ciezarowki dyspozytor

all: $(TARGETS)

magazyn: magazyn.c utils.c utils.h
	$(CC) $(CFLAGS) -o magazyn magazyn.c utils.c $(LDFLAGS)

pracownicy: pracownicy.c utils.c utils.h
	$(CC) $(CFLAGS) -o pracownicy pracownicy.c utils.c $(LDFLAGS)

pracownik4: pracownik4.c utils.c utils.h
	$(CC) $(CFLAGS) -o pracownik4 pracownik4.c utils.c $(LDFLAGS)

ciezarowki: ciezarowki.c utils.c utils.h
	$(CC) $(CFLAGS) -o ciezarowki ciezarowki.c utils.c $(LDFLAGS)

dyspozytor: dyspozytor.c utils.c utils.h
	$(CC) $(CFLAGS) -o dyspozytor dyspozytor.c utils.c $(LDFLAGS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
