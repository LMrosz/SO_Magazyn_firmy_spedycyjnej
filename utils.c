#include "utils.h"

int g_fd_log = -1;
int g_fd_sem_log = -1;
int g_semafor_log = -1;
char g_log_dir[MAX_NAZWA_PLIKU] = "";
const char *g_log_kolor = COL_RESET;

volatile sig_atomic_t g_odjedz_niepelna = 0;
volatile sig_atomic_t g_dostarcz_ekspres = 0;
volatile sig_atomic_t g_zakoncz_prace = 0;
volatile sig_atomic_t g_zakoncz_przyjmowanie = 0;

// Globalna konfiguracja z wartosciami domyslnymi
KonfiguracjaSymulacji g_config = {
    .paczek_na_ture = DOMYSLNA_PACZEK_NA_TURE,
    .interwal_generowania = DOMYSLNY_INTERWAL_GENEROWANIA,
    .liczba_ciezarowek = DOMYSLNA_LICZBA_CIEZAROWEK,
    .pojemnosc_ciezarowek = DOMYSLNA_POJEMNOSC_CIEZAROWEK,
    .waga_ciezarowek = DOMYSLNA_WAGA_CIEZAROWEK,
    .czas_rozwozu = DOMYSLNY_CZAS_ROZWOZU,
    .pojemnosc_tasmy = DOMYSLNA_POJEMNOSC_TASMY,
    .waga_tasmy = DOMYSLNA_WAGA_TASMY,
    .paczek_express = DOMYSLNA_PACZEK_EXPRESS
};

// FUNKCJA POMOCNICZA - pobieranie wartosci int
static int pobierz_int(const char *polecenie, int domyslna, int min, int max) {
    char buf[64];
    int wartosc;
    
    while (1) {
        printf("%s [%d] (zakres %d-%d): ", polecenie, domyslna, min, max);
        fflush(stdout);
        
        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            return domyslna;
        }
        
        if (buf[0] == '\n') {
            return domyslna;
        }
        
        char *endptr;
        wartosc = (int)strtol(buf, &endptr, 10);
        if (endptr == buf || (*endptr != '\n' && *endptr != '\0')) {
            printf("  Blad: Podaj liczbe calkowita!\n");
            continue;
        }
        
        if (wartosc < min || wartosc > max) {
            printf("  Blad: Wartosc musi byc w zakresie %d-%d!\n", min, max);
            continue;
        }
        
        return wartosc;
    }
}

void konfiguruj_symulacje(void) {
    printf("\n");
    printf("============================================================\n");
    printf("           KONFIGURACJA SYMULACJI MAGAZYNU\n");
    printf("      (nacisnij Enter aby uzyc wartosci domyslnej)\n");
    printf("============================================================\n");
    printf("                      PACZKI\n");
    printf("------------------------------------------------------------\n");
    
    g_config.paczek_na_ture = pobierz_int(
        "Liczba paczek generowanych na ture", 
        DOMYSLNA_PACZEK_NA_TURE, 1, MAX_PACZEK_NA_TURE);
    
    g_config.interwal_generowania = pobierz_int(
        "Interwal generowania paczek (sekundy)", 
        DOMYSLNY_INTERWAL_GENEROWANIA, 1, 180);

    g_config.paczek_express = pobierz_int(
        "Liczba paczek w pakiecie EXPRESS", 
        DOMYSLNA_PACZEK_EXPRESS, 1, 1000);
    
    printf("\n------------------------------------------------------------\n");
    printf("                     CIEZAROWKI\n");
    printf("------------------------------------------------------------\n");
    
    g_config.liczba_ciezarowek = pobierz_int(
        "Liczba ciezarowek", 
        DOMYSLNA_LICZBA_CIEZAROWEK, 1, MAX_CIEZAROWEK);
    
    g_config.waga_ciezarowek = pobierz_int(
        "Ladownosc ciezarowki (kg)", 
        DOMYSLNA_WAGA_CIEZAROWEK, 25, 25000);
    
    g_config.pojemnosc_ciezarowek = pobierz_int(
        "Pojemnosc ciezarowki (m3)", 
        DOMYSLNA_POJEMNOSC_CIEZAROWEK, 1, 100);
    
    g_config.czas_rozwozu = pobierz_int(
        "Czas rozwozu (sekundy)", 
        DOMYSLNY_CZAS_ROZWOZU, 1, 6000);
    
    printf("\n------------------------------------------------------------\n");
    printf("                       TASMA\n");
    printf("------------------------------------------------------------\n");
    
    g_config.pojemnosc_tasmy = pobierz_int(
        "Pojemnosc tasmy (liczba paczek)", 
        DOMYSLNA_POJEMNOSC_TASMY, 1, MAX_POJEMNOSC_TASMY);
  
    g_config.waga_tasmy = pobierz_int(
        "Maksymalna waga na tasmie (kg)", 
        DOMYSLNA_WAGA_TASMY, 25, 10000); 
    
    printf("\n============================================================\n");
    printf("                 PODSUMOWANIE KONFIGURACJI\n");
    printf("------------------------------------------------------------\n");
    printf(" Paczek/ture: %-6d         Interwal: %-4d s\n", 
           g_config.paczek_na_ture, g_config.interwal_generowania);
    printf(" Paczek EXPRESS w pakiecie: %-3d\n", g_config.paczek_express);
    printf("------------------------------------------------------------\n");
    printf(" Ciezarowek: %-6d          Ladownosc: %-6d kg\n", 
           g_config.liczba_ciezarowek, g_config.waga_ciezarowek);
    printf(" Pojemnosc: %-4d m3          Czas rozwozu: %-4d s\n", 
           g_config.pojemnosc_ciezarowek, g_config.czas_rozwozu);
    printf("------------------------------------------------------------\n");
    printf(" Pojemnosc tasmy: %-4d paczek    Maks. waga: %-7d kg\n", 
           g_config.pojemnosc_tasmy, g_config.waga_tasmy);
    printf("============================================================\n\n");
    
    printf("Nacisnij Enter aby rozpoczac symulacje...");
    getchar();
    printf("\n");
}

// FUNKCJE LOGI
void log_timestamp(char *buf, int size) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm = localtime(&tv.tv_sec);
    snprintf(buf, size, "[%02d:%02d:%02d.%03ld] ", tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec / 1000);
}

void log_init(int semafor, const char *nazwa_pliku, const char *kolor) {
    g_semafor_log = semafor;
    g_log_kolor = kolor;
    
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    if (g_log_dir[0] == '\0') return; 

    char sciezka[256];
    snprintf(sciezka, sizeof(sciezka), "%s/%s", g_log_dir, nazwa_pliku);
    g_fd_log = open(sciezka, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (g_fd_log == -1) {
        perror("open log");
    }
}

void log_close(void) {
    if (g_fd_log != -1) {
        close(g_fd_log);
        g_fd_log = -1;
    }
}

void log_write(const char *msg) {
    char ts[32];
    log_timestamp(ts, sizeof(ts));
    
    char full_msg[512];
    snprintf(full_msg, sizeof(full_msg), "%s%s%s%s", g_log_kolor, ts, msg, COL_RESET);
    
    fprintf(stdout, "%s", full_msg);
    fflush(stdout);
    
    if (g_fd_log != -1) {
        char file_msg[512];
        snprintf(file_msg, sizeof(file_msg), "%s%s", ts, msg);
        write(g_fd_log, file_msg, strlen(file_msg));
        fsync(g_fd_log);
    }
}

void sem_log_init(void) {
    if (g_log_dir[0] == '\0') return;
    char sciezka[256];
    snprintf(sciezka, sizeof(sciezka), "%s/semafor.log", g_log_dir);
    g_fd_sem_log = open(sciezka, O_WRONLY | O_CREAT | O_APPEND, 0644);
}

void sem_log_close(void) {
    if (g_fd_sem_log != -1) {
        close(g_fd_sem_log);
        g_fd_sem_log = -1;
    }
}

void sem_log_write(const char *msg) {
    if (g_fd_sem_log == -1) return;
    
    char ts[32];
    log_timestamp(ts, sizeof(ts));
    char full_msg[256];
    snprintf(full_msg, sizeof(full_msg), "%s%s", ts, msg);
    write(g_fd_sem_log, full_msg, strlen(full_msg));
    fsync(g_fd_sem_log);
}

void log_error(const char *msg) {
    char ts[32];
    log_timestamp(ts, sizeof(ts));
    
    char full_msg[512];
    snprintf(full_msg, sizeof(full_msg), "%s%s[BLAD] %s%s", COL_RED, ts, msg, COL_RESET);
    
    fprintf(stderr, "%s", full_msg);
    fflush(stderr);
    
    if (g_fd_log != -1) {
        char file_msg[512];
        snprintf(file_msg, sizeof(file_msg), "%s[BLAD] %s", ts, msg);
        write(g_fd_log, file_msg, strlen(file_msg));
        fsync(g_fd_log);
    }
}

// HANDLERY SYGNALOW
static void handler_sigusr1(int sig) {
    (void)sig;
    g_odjedz_niepelna = 1;
}

static void handler_sigusr2(int sig) {
    (void)sig;
    g_dostarcz_ekspres = 1;
}

static void handler_sigterm(int sig) {
    (void)sig;
    g_zakoncz_prace = 1;
}

static void handler_sigterm_ciez(int sig) {
    (void)sig;
    g_zakoncz_przyjmowanie = 1;
}

static void handler_sigchld(int sig) {
    (void)sig;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void ustaw_handler_sigchld(void) {
    signal(SIGCHLD, handler_sigchld);
}

void ustaw_handlery_ciezarowka(void) {
    signal(SIGUSR1, handler_sigusr1);
    signal(SIGTERM, handler_sigterm_ciez);
}

void ustaw_handlery_pracownik(int id) {
    signal(SIGTERM, handler_sigterm);
    if (id == 4) {
        signal(SIGUSR2, handler_sigusr2);
    }
}

// FUNKCJE SEMAFOROW
static const char* nazwa_semafora(int nr) {
    switch(nr) {
        case SEMAFOR_TASMA: return "TASMA";
        case SEMAFOR_WOLNE_MIEJSCA: return "WOLNE_MIEJSCE";
        case SEMAFOR_PACZKI: return "PACZKI";
        case SEMAFOR_CIEZAROWKI: return "CIEZAROWKI";
        case SEMAFOR_ZAPIS: return "ZAPIS";
        case SEMAFOR_EXPRESS: return "EXPRESS";
        case SEMAFOR_ID_COUNTER: return "ID_COUNTER";
        case SEMAFOR_WAGA_DOSTEPNA: return "WAGA_DOSTEPNA";
        default: return "?";
    }
}

int utworz_nowy_semafor(void) {
    int sem = semget(IPC_PRIVATE, LICZBA_SEMAFOROW, 0600 | IPC_CREAT);
    if (sem == -1) {
        perror("semget");
        return -1;
    }
    return sem;
}

void usun_semafor(int sem_id) {
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
    }
    sem_log_close();
}

void ustaw_semafor(int sem_id, int nr, int val) {
    if (semctl(sem_id, nr, SETVAL, val) == -1) {
        perror("semctl SETVAL");
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "USTAW [%s] = %d\n", nazwa_semafora(nr), val);
    sem_log_write(buf);
}

int semafor_p(int sem_id, int nr) {
    char buf[128];
    snprintf(buf, sizeof(buf), "P [%s] (PID %d) - czekam\n", nazwa_semafora(nr), getpid());
    sem_log_write(buf);
    
    struct sembuf op = { .sem_num = nr, .sem_op = -1, .sem_flg = SEM_UNDO };
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) {
            if (g_zakoncz_prace || g_zakoncz_przyjmowanie || g_odjedz_niepelna) {
                snprintf(buf, sizeof(buf), "P [%s] (PID %d) - przerwano (sygnal)\n", nazwa_semafora(nr), getpid());
                sem_log_write(buf);
                return 0;  
            }
            continue;
        }
        if (errno == EIDRM || errno == EINVAL) {
            snprintf(buf, sizeof(buf), "P [%s] (PID %d) - semafor usuniety\n", nazwa_semafora(nr), getpid());
            sem_log_write(buf);
            return 0;
        }
        perror("semop P");
        return 0;
    }
    snprintf(buf, sizeof(buf), "P [%s] (PID %d) - zablokowano\n", nazwa_semafora(nr), getpid());
    sem_log_write(buf);
    return 1;
}

int semafor_p_p4(int sem_id, int nr) {
    char buf[128];
    snprintf(buf, sizeof(buf), "P [%s] (PID %d P4) - czekam\n", nazwa_semafora(nr), getpid());
    sem_log_write(buf);
    
    struct sembuf op = { .sem_num = nr, .sem_op = -1, .sem_flg = SEM_UNDO };
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) {
            if (g_zakoncz_prace) {
                snprintf(buf, sizeof(buf), "P [%s] (PID %d P4) - przerwano (sygnal)\n", nazwa_semafora(nr), getpid());
                sem_log_write(buf);
                return 0;  
            }
            continue;
        }
        if (errno == EIDRM || errno == EINVAL) {
            snprintf(buf, sizeof(buf), "P [%s] (PID %d P4) - semafor usuniety\n", nazwa_semafora(nr), getpid());
            sem_log_write(buf);
            return 0;
        }
        perror("semop P (P4)");
        return 0;
    }
    snprintf(buf, sizeof(buf), "P [%s] (PID %d P4) - zablokowano\n", nazwa_semafora(nr), getpid());
    sem_log_write(buf);
    return 1;
}

void semafor_v(int sem_id, int nr) {
    struct sembuf op = { .sem_num = nr, .sem_op = 1, .sem_flg = SEM_UNDO };
    if (semop(sem_id, &op, 1) == -1) {
        if (errno != EIDRM && errno != EINVAL) {
            perror("semop V");
        }
        return;
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "V [%s] (PID %d) - zwolniono\n", nazwa_semafora(nr), getpid());
    sem_log_write(buf);
}

// GENERATORY
static Paczka generuj_paczke_base(int id) {
    Paczka p;
    p.id = id;
    
    int typ = rand() % 3;
    switch (typ) {
        case 0:
            p.typ = A;
            p.objetosc = VOL_A;
            p.waga = losuj_d(0.1, 5.0);
            break;
        case 1:
            p.typ = B;
            p.objetosc = VOL_B;
            p.waga = losuj_d(5.0, 15.0);
            break;
        default:
            p.typ = C;
            p.objetosc = VOL_C;
            p.waga = losuj_d(15.0, 24.9);
            break;
    }
    p.waga = round(p.waga * 1000) / 1000.0;
    return p;
}

Paczka generuj_paczke_zwykla(int id) {
    Paczka p = generuj_paczke_base(id);
    p.priorytet = ZWYKLA;
    return p;
}

Paczka generuj_paczke_ekspres(int id) {
    Paczka p = generuj_paczke_base(id);
    p.priorytet = EXPRES;
    return p;
}

int pobierz_nastepne_id(int semafor, LicznikId *licznik) {
    semafor_p(semafor, SEMAFOR_ID_COUNTER);
    int id = licznik->nastepne_id++;
    semafor_v(semafor, SEMAFOR_ID_COUNTER);
    return id;
}

void generuj_tasme(Tasma* tasma) {
    tasma->head = 0;
    tasma->tail = 0;
    tasma->aktualna_ilosc = 0;
    tasma->aktualna_waga = 0;
    tasma->max_pojemnosc = g_config.pojemnosc_tasmy;
    tasma->max_waga = g_config.waga_tasmy;
    tasma->ciezarowka = 0;

    char buf[256];
    snprintf(buf, sizeof(buf), "Tasma: K=%d paczek, M=%dkg\n", tasma->max_pojemnosc, tasma->max_waga);
    log_write(buf);
}

Ciezarowka* generuj_ciezarowke(int *liczba_ciezarowek_out) {
    int n = g_config.liczba_ciezarowek;
    
    Ciezarowka *ciezarowki = malloc(n * sizeof(Ciezarowka));
    if (!ciezarowki) {
        perror("malloc ciezarowki");
        return NULL;
    }
    
    if (liczba_ciezarowek_out) {
        *liczba_ciezarowek_out = n;
    }
    
    for (int i = 0; i < n; i++) {
        ciezarowki[i].id_ciezarowki = i + 1;
        ciezarowki[i].waga_ciezarowki = g_config.waga_ciezarowek;
        ciezarowki[i].pojemnosc_ciezarowki = g_config.pojemnosc_ciezarowek;
        ciezarowki[i].czas_rozwozu = g_config.czas_rozwozu;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "Ciezarowki: N=%d, W=%dkg, V=%dm3, Ti=%ds\n", n, g_config.waga_ciezarowek, g_config.pojemnosc_ciezarowek, g_config.czas_rozwozu);
    log_write(buf);

    return ciezarowki;
}

// KOLEJKI KOMUNIKATOW
int utworz_kolejke(void) {
    key_t klucz = ftok(".", 'Q');
    if (klucz == -1) {
        perror("ftok kolejka");
        return -1;
    }
    int msgid = msgget(klucz, IPC_CREAT | 0600);
    if (msgid == -1) {
        perror("msgget");
    }
    return msgid;
}

void usun_kolejke(int msgid) {
    if (msgid != -1) {
        msgctl(msgid, IPC_RMID, NULL);
    }
}
