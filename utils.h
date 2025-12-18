#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/sem.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>

#define VOL_A 0.019456
#define VOL_B 0.046208
#define VOL_C 0.099712

#define MAX_PACZEK 18000
#define MAX_POJEMNOSC_FIZYCZNA_TASMY 50
#define LICZBA_PRACOWNIKOW 3
#define MAX_NAZWA_PLIKU 64
#define MAX_LOG_BUFOR 1024

//ZMIENNE GLOBALNE
extern int g_fd_wyniki;
extern int g_semafor_log;
extern char g_nazwa_pliku[MAX_NAZWA_PLIKU];

//TYPY I STRUKTURY
typedef enum { A, B, C } TypPaczki;
typedef enum { ZWYKLA, EXPRES } Priorytet;

typedef struct {
    int id;
    double waga;
    TypPaczki typ;
    double objetosc;
    Priorytet priorytet;
} Paczka;

typedef struct {
    int id_ciezarowki;
    int waga_ciezarowki;
    int pojemnosc_ciezarowki;
    time_t czas_rozwozu;
} Ciezarowka;

typedef struct {
    int liczba_paczek;
    int aktywny;
    Paczka magazyn[MAX_PACZEK];
} Magazyn_wspolny;

typedef struct {
    Paczka bufor[MAX_POJEMNOSC_FIZYCZNA_TASMY];
    int head;
    int tail;
    int aktualna_ilosc;
    double aktualna_waga;
    int max_pojemnosc;
    int max_waga;
} Tasma;

//FUNKCJE INLINE
static inline int losuj(int min, int max) {
    if (min > max) return min;
    return rand() % (max - min + 1) + min;
}

static inline double losuj_d(double min, double max) {
    return min + (double)rand() / RAND_MAX * (max - min);
}

static inline const char* nazwa_typu(TypPaczki t) {
    switch (t) {
        case A: return "A (Mala)";
        case B: return "B (Srednia)";
        case C: return "C (Duza)";
        default: return "?";
    }
}

static inline const char* nazwa_priorytetu(Priorytet p) {
    return (p == EXPRES) ? "EKSPRES" : "Zwykla";
}

static inline double objetosc_typu(TypPaczki typ) {
    switch (typ) {
        case A: return VOL_A;
        case B: return VOL_B;
        case C: return VOL_C;
        default: return 0;
    }
}

static inline bool czy_pojemnosc(TypPaczki typ, double wolne_miejsce) {
    return objetosc_typu(typ) <= wolne_miejsce;
}

//DEKLARACJE FUNKCJI
// Semafory
int utworz_nowy_semafor(void);
void usun_semafor(int sem_id);
void ustaw_semafor(int sem_id, int nr, int val);
void semafor_p(int sem_id, int nr);
void semafor_v(int sem_id, int nr);

// Generatory
Paczka* generuj_paczke(int *liczba_paczek_out);
Ciezarowka* generuj_ciezarowke(int *liczba_ciezarowek_out);
void generuj_tasme(Tasma* tasma);

//Obsługa plikow
int otworz_plik_wyniki(int semafor);
void zamknij_plik_wyniki(void);
void logi(const char *format, ...); 
#endif