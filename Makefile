magazyn: magazyn.c dyspozytor.o pracownicy.o ciezarowki.o
	gcc magazyn.c -o magazyn -lm

dyspozytor.o: dyspozytor.c utils.h
	gcc dyspozytor.c -o dyspozytor

pracownicy.o: pracownicy.c
	gcc pracownicy.c -o pracownicy -lm

ciezarowki.o: ciezarowki.c
	gcc ciezarowki.c -o ciezarowki
