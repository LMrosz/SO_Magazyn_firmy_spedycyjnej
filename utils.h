#ifndef UTILS_H
#define UTILS_H

// === BIBLIOTEKI STANDARDOWE ===
#include <stdio.h>      // printf, fprintf, fopen, fclose, snprintf, perror
#include <stdlib.h>     // malloc, free, realloc, atoi, rand, srand, exit
#include <string.h>     // strcpy, strncpy, strlen, memset, strstr, strcmp
#include <time.h>       // time, localtime, struct tm
#include <math.h>       // round
#include <unistd.h>     // fork, exec, sleep, usleep, getpid, write, read, close
#include <stdbool.h>    // bool, true, false
#include <errno.h>      // errno, EINTR, EAGAIN, EIDRM, EINVAL

// === BIBLIOTEKI SYSTEMOWE - IPC SYSTEM V ===
#include <sys/types.h>  // pid_t, key_t, size_t
#include <sys/ipc.h>    // IPC_PRIVATE, IPC_CREAT, IPC_RMID, ftok
#include <sys/shm.h>    // shmget, shmat, shmdt, shmctl - pamiec dzielona
#include <sys/sem.h>    // semget, semop, semctl - semafory
#include <sys/msg.h>    // msgget, msgsnd, msgrcv, msgctl - kolejki komunikatow
#include <sys/wait.h>   // wait, waitpid, WNOHANG
#include <sys/stat.h>   // mkdir,
#include <sys/time.h>   // gettimeofday, struct timeval

// === BIBLIOTEKI SYSTEMOWE - SYGNALY I PLIKI ===
#include <signal.h>     // signal, sigaction, kill, SIGUSR1, SIGUSR2, SIGTERM, SIGCHLD
#include <fcntl.h>      // open, O_WRONLY, O_CREAT, O_APPEND
#include <dirent.h>     // opendir, readdir, closedir - przegladanie /proc

//STAŁE POJEMNOSCI PACZEK W m^3
#define VOL_A 0.019456
#define VOL_B 0.046208
#define VOL_C 0.099712

//STAŁE - ograniczone żeby uniknąć przepełnienia semafora (limit 32767)
#define MAX_PACZEK 5000
#define MAX_POJEMNOSC_TASMY 250
#define MAX_CIEZAROWEK 1000
#define LICZBA_PRACOWNIKOW 4
#define MAX_NAZWA_PLIKU 64
  
//DANE DO SYMULACJI - wartosci domyslne
#define DOMYSLNA_PACZEK_NA_TURE 50
#define DOMYSLNY_INTERWAL_GENEROWANIA 5
#define DOMYSLNA_LICZBA_CIEZAROWEK 5
#define DOMYSLNA_POJEMNOSC_CIEZAROWEK 10      
#define DOMYSLNA_WAGA_CIEZAROWEK 1000         
#define DOMYSLNY_CZAS_ROZWOZU 30
#define DOMYSLNA_POJEMNOSC_TASMY 25           
#define DOMYSLNA_WAGA_TASMY 500             
#define DOMYSLNY_PROCENT_EKSPRES 20          

//SEMAFORY
#define SEMAFOR_TASMA 0             // mutex - dostep do struktury tasmy
#define SEMAFOR_WOLNE_MIEJSCA 1     // licznik - wolne miejsca na tasmie
#define SEMAFOR_PACZKI 2            // licznik - paczki gotowe do zabrania
#define SEMAFOR_CIEZAROWKI 3        // mutex - stanowisko przy tasmie
#define SEMAFOR_ZAPIS 4             // mutex - zapis do logow
#define SEMAFOR_EXPRESS 5           // mutex - okienko ekspresowe
#define SEMAFOR_ID_COUNTER 6        // mutex - licznik ID paczek
#define LICZBA_SEMAFOROW 7

// === KOLEJKI KOMUNIKATOW ===
#define MSG_CIEZAROWKA_PRZY_TASMIE 1 
#define MSG_P4_ODPOWIEDZ 2            

typedef struct {
    long mtype;
    pid_t ciezarowka_pid;
} MsgCiezarowkaPrzyTasmie;

typedef struct {
    long mtype;
    int odebrano_wszystko;
    int ile_zostalo;
} MsgP4Odpowiedz;

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
    int procent_ekspres;
} KonfiguracjaSymulacji;

//ZMIENNE GLOBALNE
extern int g_fd_log;
extern int g_fd_sem_log;
extern int g_semafor_log;
extern char g_log_dir[MAX_NAZWA_PLIKU];
extern const char *g_log_kolor;
extern KonfiguracjaSymulacji g_config;

//ZMIENNE GLOBALNE DO OBSŁUGI SYGNAŁÓW - FLAGI
//zmienna globalna ktora nie jest przechowywana w pamieci "volatile" moze byc zmieniona w dowolnym momecie
extern volatile sig_atomic_t g_odjedz_niepelna;     // odjedź z niepełnym ładunkiem
extern volatile sig_atomic_t g_dostarcz_ekspres;    // P4 ma dostarczyć ekspres
extern volatile sig_atomic_t g_zakoncz_prace;       // zakończ pracę
extern volatile sig_atomic_t g_zakoncz_przyjmowanie;// konczy przyjmowanie / generowanie nowych paczek

//FUNKCJE INLINE
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

//DEKLARACJE FUNKCJI
void konfiguruj_symulacje(void); //konfiguracja symulacji
// Semafory
int utworz_nowy_semafor(void);
void usun_semafor(int sem_id);
void ustaw_semafor(int sem_id, int nr, int val);
int semafor_p(int sem_id, int nr);
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

//Obsługa sygnałów i sprzatanie
void ustaw_handlery_ciezarowka(void);
void ustaw_handlery_pracownik(int id_pracownika);
void ustaw_handler_sigchld(void);

//Kolejki komunikatow
int utworz_kolejke(void);
void usun_kolejke(int msgid);
int wyslij_msg_ciezarowka(int msgid, pid_t pid);
int wyslij_msg_odpowiedz(int msgid, int wszystko, int zostalo);
int odbierz_msg_ciezarowka(int msgid, MsgCiezarowkaPrzyTasmie *msg);
int odbierz_msg_odpowiedz(int msgid, MsgP4Odpowiedz *msg);
#endif