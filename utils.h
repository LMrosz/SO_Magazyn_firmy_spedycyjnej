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
#include <sys/select.h>
#include <termios.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/stat.h>

#define VOL_A 0.019456
#define VOL_B 0.046208
#define VOL_C 0.099712

//STAŁE?
#define MAX_PACZEK 10000
#define LICZBA_PRACOWNIKOW 4
#define MAX_NAZWA_PLIKU 64
#define MAX_LOG_BUFOR 1024
#define MAX_EKSPRES MAX_PACZEK   

//DANE DO SYMULACJI
#define LICZBA_PACZEK_START 100   // początkowa liczba paczek
#define PACZEK_NA_TURE 10          // ile paczek generować na turę
#define INTERWAL_GENEROWANIA 5    // czas generowania nowej tury paczek
#define LICZBA_CIEZAROWEK 50      // liczba ciezarowek
#define POJEMNOSC_CIEZAROWEK 100  // pojemnosc ciezarowek w m^3
#define WAGA_CIEZAROWEK 1000     // dopuszczalna waga ciezarowki
#define CZAS_ROZWOZU 60           // czas rozwozu ciezarowki
#define POJEMNOSC_TASMY 25        // maksymalna ilosc paczek na tasmie
#define WAGA_TASMY 250            // maksymalna waga paczek na tasmie

//SEMAFORY
#define SEMAFOR_MAGAZYN 0        // dostep do magazynu (mutex)
#define SEMAFOR_TASMA 1          // dostep do tasmy (mutex)
#define SEMAFOR_WOLNE_MIEJSCA 2  // licznik wolnych miejsc (liczenie w dol)
#define SEMAFOR_PACZKI 3         // licznik paczek na tasmie (liczenie w gore)
#define SEMAFOR_CIEZAROWKI 4     // dostep do tasmy ciezarowek (mutex)
#define SEMAFOR_ZAPIS 5          // wypisywanie na ekran i do pliku (mutex)
#define SEMAFOR_EXPRESS 6        // obsluga paczek ekspresowych
#define SEMAFOR_GENERATOR 7      // mutex dla generowania nowych paczek

//SYGNAŁY DYSPOZYTORRA
#define SYGNAL_ODJEDZ_NIEPELNA SIGUSR1  // ciezarowka odjezdza z niepełnym ładunkiem
#define SYGNAL_DOSTARCZ_EKSPRES SIGUSR2 // pracownik 4 dostarcza ładunki express do ciezarowki przy tasmie
#define SYGNAL_ZAKONCZ_PRZYJMOWANIE SIGTERM          // zakonczenie pracy

//KOLORY ANSI
#define COL_RESET   "\033[0m"
#define COL_RED     "\033[31m"
#define COL_GREEN   "\033[32m"
#define COL_YELLOW  "\033[33m"
#define COL_MAGENTA "\033[35m"
#define COL_CYAN    "\033[36m"

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
    int nastepne_id;                
    int generowanie_aktywne;          
    Paczka magazyn[MAX_PACZEK];
} Magazyn_wspolny;

typedef struct {
    pid_t pid;
    int id;
    double aktualna_waga;
    double aktualna_pojemnosc;
    int max_waga;
    int max_pojemnosc;
    int liczba_paczek;
} CiezarowkaInfo;

typedef struct {
    Paczka bufor[POJEMNOSC_TASMY];
    int head;
    int tail;
    int aktualna_ilosc;
    double aktualna_waga;
    int max_pojemnosc;
    int max_waga;
    pid_t ciezarowka; //pid ciezarowki przy tasmie
    CiezarowkaInfo ciezarowka_info;
} Tasma;

typedef struct {
    Paczka *paczki;                   
    int ilosc;
    int max_ilosc;                    
    pid_t ciezarowka_pid;
    int gotowe;
} OkienkoEkspres;

typedef struct {
    Paczka paczki[MAX_PACZEK];
    int ilosc;
    pid_t ciezarowka_pid;
    int gotowe;
} OkienkoEkspresShm;

//ZMIENNE GLOBALNE
extern int g_fd_log;
extern int g_semafor_log;
extern char g_log_dir[MAX_NAZWA_PLIKU];
extern const char *g_log_kolor;

//ZMIENNE GLOBALNE DO OBSŁUGI SYGNAŁÓW - FLAGI
//zmienna globalna ktora nie jest przechowywana w pamieci "volatile" moze byc zmieniona w dowolnym momecie
extern volatile sig_atomic_t g_odjedz_niepelna;  // odjedź z niepełnym ładunkiem
extern volatile sig_atomic_t g_dostarcz_ekspres; // P4 ma dostarczyć ekspres
extern volatile sig_atomic_t g_zakoncz_prace;    // zakończ pracę
extern volatile sig_atomic_t g_zakoncz_przyjmowanie; // konczy przyjmowanie / generowanie nowych paczek

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
Paczka* generuj_paczke_poczatkowe(int *liczba_paczek_out, int *nastepne_id);
Paczka generuj_pojedyncza_paczke(int id);
Ciezarowka* generuj_ciezarowke(int *liczba_ciezarowek_out);
void generuj_tasme(Tasma* tasma);

// Raporty
void log_init(int semafor, const char *nazwa_pliku, const char *kolor);
void log_close(void);
void log_write(const char *msg);
void log_timestamp(char *buf, int size);
void sem_log_init(void);
void sem_log_close(void);
void sem_log_write(const char *msg);

//Obsługa sygnałów i sprzatanie
void ustaw_handlery_ciezarowka(void);
void ustaw_handlery_pracownik(int id_pracownika);
void ustaw_handlery_generator(void);

#endif
