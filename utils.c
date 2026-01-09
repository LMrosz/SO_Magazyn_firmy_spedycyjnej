#include "utils.h"

int g_fd_log = -1;
int g_semafor_log = -1;
char g_log_dir[MAX_NAZWA_PLIKU] = "";
const char *g_log_kolor = COL_RESET;
static int g_fd_sem_log = -1;

volatile sig_atomic_t g_odjedz_niepelna = 0;
volatile sig_atomic_t g_dostarcz_ekspres = 0;
volatile sig_atomic_t g_zakoncz_prace = 0;
volatile sig_atomic_t g_zakoncz_przyjmowanie = 0;

//FUNKCJE POMOCNICZE - zapis na ekran i do plikow
void log_timestamp(char *buf, int size) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm = localtime(&tv.tv_sec);
    snprintf(buf, size, "[%02d:%02d:%02d.%03ld] ",
             tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec / 1000);
}

void log_init(int semafor, const char *nazwa_pliku, const char *kolor) {
    g_semafor_log = semafor;
    g_log_kolor = kolor;
    
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
    
    if (g_semafor_log != -1) {
        semafor_p(g_semafor_log, SEMAFOR_ZAPIS);
    }
    
    write(STDOUT_FILENO, g_log_kolor, strlen(g_log_kolor));
    write(STDOUT_FILENO, ts, strlen(ts));
    write(STDOUT_FILENO, msg, strlen(msg));
    write(STDOUT_FILENO, COL_RESET, strlen(COL_RESET));
    
    if (g_fd_log != -1) {
        write(g_fd_log, ts, strlen(ts));
        write(g_fd_log, msg, strlen(msg));
    }
    
    if (g_semafor_log != -1) {
        semafor_v(g_semafor_log, SEMAFOR_ZAPIS);
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
    
    write(g_fd_sem_log, ts, strlen(ts));
    write(g_fd_sem_log, msg, strlen(msg));
}

//HANDLERY SYGNAŁÓW

void handler_odjedz_niepelna(int sig) {
    (void)sig;
    g_odjedz_niepelna = 1;
}

void handler_dostarcz_ekspres(int sig) {
    (void)sig;
    g_dostarcz_ekspres = 1;
}

void handler_zakoncz(int sig) {
    (void)sig;
    g_zakoncz_prace = 1;
}

void handler_zakoncz_przyjmowanie(int sig) {
    (void)sig;
    g_zakoncz_przyjmowanie = 1;
}

void ustaw_handlery_ciezarowka(void) {
    struct sigaction sa;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler_odjedz_niepelna;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler_zakoncz_przyjmowanie;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler_zakoncz;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    
    signal(SIGUSR2, SIG_IGN);
}

void ustaw_handlery_pracownik(int id_pracownika) {
    struct sigaction sa;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler_zakoncz;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    
    if (id_pracownika == 4) {
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = handler_dostarcz_ekspres;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGUSR2, &sa, NULL);
    } else {
        signal(SIGUSR2, SIG_IGN);
    }
    
    signal(SIGUSR1, SIG_IGN);
}

void ustaw_handlery_generator(void) {
    struct sigaction sa;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler_zakoncz;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);
}

//FUNKCJE - SEMAFOR
static const char* nazwa_semafora(int nr) {
    switch(nr) {
        case SEMAFOR_MAGAZYN: return "MAGAZYN";
        case SEMAFOR_TASMA: return "TASMA";
        case SEMAFOR_WOLNE_MIEJSCA: return "WOLNE_MIEJSCE";
        case SEMAFOR_PACZKI: return "PACZKI_TASMA";
        case SEMAFOR_CIEZAROWKI: return "CIEZAROWKI";
        case SEMAFOR_ZAPIS: return "ZAPIS";
        case SEMAFOR_EXPRESS: return "PACZKI_EXPRESS";
        case SEMAFOR_GENERATOR: return "GENERATOR";
        default: return "?";
    }
}

int utworz_nowy_semafor(void) {
    key_t klucz = ftok(".", 'S');
    if (klucz == -1) {
        perror("ftok semafor");
        exit(EXIT_FAILURE);
    }
    int sem = semget(klucz, 8, 0600 | IPC_CREAT);
    if (sem == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
    char buf[128];
    snprintf(buf, sizeof(buf),"Semafor zostal utworzony : %d\n", sem);
    sem_log_write(buf);
    return sem;
}

void usun_semafor(int sem_id) {
    char buf[128];
    snprintf(buf, sizeof(buf), "USUWANIE semafora %d\n", sem_id);
    sem_log_write(buf);

    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
    } else {
        snprintf(buf, sizeof(buf), "Semafor %d zostal usuniety.\n", sem_id);
        sem_log_write(buf);
    }
    sem_log_close();
}

void ustaw_semafor(int sem_id, int nr, int val) {
    if (semctl(sem_id, nr, SETVAL, val) == -1) {
        perror("semctl SETVAL");
        exit(EXIT_FAILURE);
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "USTAW [%s] = %d\n", nazwa_semafora(nr), val);
    sem_log_write(buf);
}

void semafor_p(int sem_id, int nr) {
    if (nr != SEMAFOR_ZAPIS) {
        char buf[128];
        snprintf(buf, sizeof(buf), "P [%s] (PID %d)\n", nazwa_semafora(nr), getpid());
        sem_log_write(buf);
    }

    struct sembuf op = { .sem_num = nr, .sem_op = -1, .sem_flg = SEM_UNDO };
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) continue;
        perror("semop P");
        exit(EXIT_FAILURE);
    }
}

void semafor_v(int sem_id, int nr) {
    struct sembuf op = { .sem_num = nr, .sem_op = 1, .sem_flg = SEM_UNDO };
    if (semop(sem_id, &op, 1) == -1) {
        perror("semop V");
        exit(EXIT_FAILURE);
    }

    if (nr != SEMAFOR_ZAPIS) {
        char buf[128];
        snprintf(buf, sizeof(buf), "V [%s] (PID %d)\n", nazwa_semafora(nr), getpid());
        sem_log_write(buf);
    }
}

//GENERATORY
// Generuje pojedynczą paczkę o podanym ID
Paczka generuj_pojedyncza_paczke(int id) {
    Paczka p;
    p.id = id;
    
    int typ_rand = rand() % 3;
    switch (typ_rand) {
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
            p.waga = losuj_d(15.0, 25.0);
            break;
    }
    p.waga = round(p.waga * 1000) / 1000.0;
    p.priorytet = (rand() % 100 < 20) ? EXPRES : ZWYKLA; // 20% szans na ekspres
    char buf[256];

    snprintf(buf, sizeof(buf),"Paczka %d  |  Typ paczki: %s  |  Objetosc paczki: %lf  |  Waga paczki: %lf\n",p.id,p.priorytet == EXPRES ? "EKSPRES" : "zwykla",p.objetosc, p.waga);
    log_write(buf);
    return p;
}

// Generuje początkowy zestaw paczek
Paczka* generuj_paczke_poczatkowe(int *liczba_paczek_out, int *nastepne_id) {
    int liczba_paczek = LICZBA_PACZEK_START;
    
    Paczka *magazyn = (Paczka*)malloc(liczba_paczek * sizeof(Paczka));
    if (!magazyn) {
        perror("malloc magazyn");
        return NULL;
    }
    
    if (liczba_paczek_out) {
        *liczba_paczek_out = liczba_paczek;
    }
    
    int ekspresowe = 0;
    
    for (int i = 0; i < liczba_paczek; i++) {
        magazyn[i] = generuj_pojedyncza_paczke(i + 1);
        if (magazyn[i].priorytet == EXPRES) ekspresowe++;
    }
    
    if (nastepne_id) {
        *nastepne_id = liczba_paczek + 1;
    }

    char buf[256];
    snprintf(buf, sizeof(buf),"\n-------------GENEROWANIE PACZEK-------------\n");
    log_write(buf);
    snprintf(buf, sizeof(buf),"Wygenerowano %d paczek poczatkowych\n", liczba_paczek);
    log_write(buf);
    snprintf(buf, sizeof(buf),"W tym ekspresowych: %d\n", ekspresowe);
    log_write(buf);
    snprintf(buf, sizeof(buf),"Generowanie dynamiczne: AKTYWNE (co %d sekund, %d paczek)\n\n", 
         INTERWAL_GENEROWANIA, PACZEK_NA_TURE);
    log_write(buf);

    return magazyn;
}

void generuj_tasme(Tasma* tasma) {
    tasma->head = 0;
    tasma->tail = 0;
    tasma->aktualna_ilosc = 0;
    tasma->aktualna_waga = 0;
    tasma->max_pojemnosc = POJEMNOSC_TASMY;
    tasma->max_waga = WAGA_TASMY;
    tasma->ciezarowka = 0;

    char buf[256];
    snprintf(buf, sizeof(buf),"\n-------------GENEROWANIE TASMY-------------\n");
    log_write(buf);
    snprintf(buf, sizeof(buf),"Maksymalna liczba paczek: %d\n", tasma->max_pojemnosc);
    log_write(buf);
    snprintf(buf, sizeof(buf),"Maksymalny udzwig: %d kg\n\n", tasma->max_waga);
    log_write(buf);
}

Ciezarowka* generuj_ciezarowke(int *liczba_ciezarowek_out) {
    int liczba_ciezarowek = LICZBA_CIEZAROWEK;
    int waga_ciezarowki = WAGA_CIEZAROWEK;
    int pojemnosc_ciezarowki = POJEMNOSC_CIEZAROWEK;
    time_t czas_ciezarowki = CZAS_ROZWOZU;
    
    Ciezarowka *ciezarowki = (Ciezarowka*)malloc(liczba_ciezarowek * sizeof(Ciezarowka));
    if (!ciezarowki) {
        perror("malloc ciezarowki");
        return NULL;
    }
    
    if (liczba_ciezarowek_out) {
        *liczba_ciezarowek_out = liczba_ciezarowek;
    }
    
    for (int i = 0; i < liczba_ciezarowek; i++) {
        ciezarowki[i].id_ciezarowki = i + 1;
        ciezarowki[i].waga_ciezarowki = waga_ciezarowki;
        ciezarowki[i].pojemnosc_ciezarowki = pojemnosc_ciezarowki;
        ciezarowki[i].czas_rozwozu = czas_ciezarowki;
    }

    char buf[256];
    
    snprintf(buf, sizeof(buf),"\n-------------GENEROWANIE CIEZAROWEK-------------\n");
    log_write(buf);
    snprintf(buf, sizeof(buf),"Liczba ciezarowek: %d\n", liczba_ciezarowek);
    log_write(buf);
    snprintf(buf, sizeof(buf),"Ladownosc: %d kg\n", waga_ciezarowki);
    log_write(buf);
    snprintf(buf, sizeof(buf),"Pojemnosc: %d m^3\n", pojemnosc_ciezarowki);
    log_write(buf);
    snprintf(buf, sizeof(buf),"Czas rozwozu: %ld s\n\n", czas_ciezarowki);
    log_write(buf);

    return ciezarowki;
}