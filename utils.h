#ifndef UTILS_H
#define UTILS_H

// === BIBLIOTEKI STANDARDOWE ===
#include <stdio.h>      
#include <stdlib.h>     
#include <string.h>     
#include <time.h>       
#include <math.h>       
#include <unistd.h>     
#include <stdbool.h>    
#include <errno.h>      

// === BIBLIOTEKI SYSTEMOWE - IPC SYSTEM V ===
#include <sys/types.h>  
#include <sys/ipc.h>    
#include <sys/shm.h>    
#include <sys/sem.h>    
#include <sys/msg.h>    
#include <sys/wait.h>   
#include <sys/stat.h>   
#include <sys/time.h>   

// === BIBLIOTEKI SYSTEMOWE - SYGNALY I PLIKI ===
#include <signal.h>     
#include <fcntl.h>     

// STALE POJEMNOSCI PACZEK W m^3
#define VOL_A 0.019456  // A (64x38x8 cm)
#define VOL_B 0.046208  // B (64x38x19 cm)
#define VOL_C 0.099712  // C (64x38x41 cm)

// STALE
#define MAX_PACZEK 5000
#define MAX_POJEMNOSC_TASMY 250
#define MAX_CIEZAROWEK 1000
#define LICZBA_PRACOWNIKOW 4
#define MAX_NAZWA_PLIKU 64
  
// DANE DO SYMULACJI - wartosci domyslne
#define DOMYSLNA_PACZEK_NA_TURE 1000
#define MAX_PACZEK_NA_TURE 1000
#define DOMYSLNY_INTERWAL_GENEROWANIA 2
#define DOMYSLNA_LICZBA_CIEZAROWEK 100
#define DOMYSLNA_POJEMNOSC_CIEZAROWEK 5     
#define DOMYSLNA_WAGA_CIEZAROWEK 1000         
#define DOMYSLNY_CZAS_ROZWOZU 45
#define DOMYSLNA_POJEMNOSC_TASMY 100           
#define DOMYSLNA_WAGA_TASMY 2000           
#define DOMYSLNA_PACZEK_EXPRESS 1000 

// SEMAFORY
#define SEMAFOR_TASMA 0             // mutex - dostep do struktury tasmy
#define SEMAFOR_WOLNE_MIEJSCA 1     // licznik - wolne miejsca na tasmie
#define SEMAFOR_PACZKI 2            // licznik - paczki gotowe do zabrania
#define SEMAFOR_CIEZAROWKI 3        // mutex - stanowisko przy tasmie
#define SEMAFOR_ZAPIS 4             // mutex - zapis do logow
#define SEMAFOR_EXPRESS 5           // mutex - okienko ekspresowe
#define SEMAFOR_ID_COUNTER 6        // mutex - licznik ID paczek
#define LICZBA_SEMAFOROW 7

// KOLEJKI KOMUNIKATOW - P4 - Ciezarowka
#define MSG_P4_DOSTAWA_GOTOWA     10  // P4 -> ciezarowka: mam paczki do dostarczenia
#define MSG_CIEZAROWKA_GOTOWA     11  // ciezarowka -> P4: jestem gotowa na odbior
#define MSG_P4_PACZKI_PRZEKAZANE  12  // P4 -> ciezarowka: paczki w okienku
#define MSG_ODEBRANO_POTWIERDZENIE 13 // ciezarowka -> P4: odebralem X paczek

// Struktura komunikatu od P4
typedef struct {
    long mtype;
    int ile_paczek;
    pid_t nadawca_pid;
} MsgP4Dostawa;

// Struktura potwierdzenia od ciezarowki
typedef struct {
    long mtype;
    int ile_odebranych;
    int ile_zostalo;
    int pojemnosc_wolna;
} MsgPotwierdzenie;

// KOLORY ANSI
#define COL_RESET   "\033[0m"
#define COL_RED     "\033[31m"
#define COL_GREEN   "\033[32m"
#define COL_YELLOW  "\033[33m"
#define COL_MAGENTA "\033[35m"
#define COL_CYAN    "\033[36m"

// TYPY I STRUKTURY
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
    int nastepne_id;
    int generowanie_aktywne;
} LicznikId;

typedef struct {
    Paczka bufor[MAX_POJEMNOSC_TASMY];
    int head;
    int tail;
    int aktualna_ilosc;
    double aktualna_waga;
    int max_pojemnosc;
    int max_waga;
    pid_t ciezarowka; 
    pid_t magazyn_pid;
    pid_t dyspozytor_pid;
} Tasma;

typedef struct {
    Paczka paczki[MAX_PACZEK];
    int ilosc;
    pid_t ciezarowka_pid;
    int gotowe;              
} OkienkoEkspresShm;

typedef struct {
    int paczek_na_ture;
    int interwal_generowania;
    int liczba_ciezarowek;
    int pojemnosc_ciezarowek;
    int waga_ciezarowek;
    int czas_rozwozu;
    int pojemnosc_tasmy;
    int waga_tasmy;
    int paczek_express;  
} KonfiguracjaSymulacji;

// ZMIENNE GLOBALNE
extern int g_fd_log;
extern int g_fd_sem_log;
extern int g_semafor_log;
extern char g_log_dir[MAX_NAZWA_PLIKU];
extern const char *g_log_kolor;
extern KonfiguracjaSymulacji g_config;

// ZMIENNE GLOBALNE DO OBSLUGI SYGNALOW - FLAGI
extern volatile sig_atomic_t g_odjedz_niepelna;     // odjezdz z niepelnym ladunkiem
extern volatile sig_atomic_t g_dostarcz_ekspres;    // P4 ma dostarczyc ekspres
extern volatile sig_atomic_t g_zakoncz_prace;       // zakoncz prace
extern volatile sig_atomic_t g_zakoncz_przyjmowanie;// konczy przyjmowanie

// FUNKCJE INLINE
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

static inline double objetosc_typu(TypPaczki typ) {
    switch (typ) {
        case A: return VOL_A;
        case B: return VOL_B;
        case C: return VOL_C;
        default: return 0;
    }
}

// DEKLARACJE FUNKCJI
void konfiguruj_symulacje(void);

// Semafory
int utworz_nowy_semafor(void);
void usun_semafor(int sem_id);
void ustaw_semafor(int sem_id, int nr, int val);
int semafor_p(int sem_id, int nr);
int semafor_p_p4(int sem_id, int nr);
void semafor_v(int sem_id, int nr);

// Generatory
Paczka generuj_paczke_zwykla(int id);
Paczka generuj_paczke_ekspres(int id);
Ciezarowka* generuj_ciezarowke(int *liczba_ciezarowek_out);
void generuj_tasme(Tasma* tasma);
int pobierz_nastepne_id(int semafor, LicznikId *licznik);

// Raporty
void log_init(int semafor, const char *nazwa_pliku, const char *kolor);
void log_close(void);
void log_write(const char *msg);
void log_timestamp(char *buf, int size);
void sem_log_init(void);
void sem_log_close(void);
void sem_log_write(const char *msg);
void log_error(const char *msg);

// Obsluga sygnalow
void ustaw_handlery_ciezarowka(void);
void ustaw_handlery_pracownik(int id_pracownika);
void ustaw_handler_sigchld(void);

// Kolejki komunikatow
int utworz_kolejke(void);
void usun_kolejke(int msgid);

#endif
