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

// Globalna konfiguracja z wartościami domyślnymi
KonfiguracjaSymulacji g_config = {
    .paczek_na_ture = DOMYSLNA_PACZEK_NA_TURE,
    .interwal_generowania = DOMYSLNY_INTERWAL_GENEROWANIA,
    .liczba_ciezarowek = DOMYSLNA_LICZBA_CIEZAROWEK,
    .pojemnosc_ciezarowek = DOMYSLNA_POJEMNOSC_CIEZAROWEK,
    .waga_ciezarowek = DOMYSLNA_WAGA_CIEZAROWEK,
    .czas_rozwozu = DOMYSLNY_CZAS_ROZWOZU,
    .pojemnosc_tasmy = DOMYSLNA_POJEMNOSC_TASMY,
    .waga_tasmy = DOMYSLNA_WAGA_TASMY,
    .procent_ekspres = DOMYSLNY_PROCENT_EKSPRES
};

// FUNKCJA POMOCNICZA - pobieranie wartosci int i walidacja danych
static int pobierz_int(const char *prompt, int domyslna, int min, int max) {
    char buf[64];
    int wartosc;
    
    while (1) {
        printf("%s [%d] (zakres %d-%d): ", prompt, domyslna, min, max);
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
            printf("  Błąd: Podaj liczbę całkowitą!\n");
            continue;
        }
        
        if (wartosc < min || wartosc > max) {
            printf("  Błąd: Wartość musi być w zakresie %d-%d!\n", min, max);
            continue;
        }
        
        return wartosc;
    }
}

void konfiguruj_symulacje(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           KONFIGURACJA SYMULACJI MAGAZYNU                    ║\n");
    printf("║      (naciśnij Enter aby użyć wartości domyślnej)            ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║                      PACZKI                                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    g_config.paczek_na_ture = pobierz_int(
        "Liczba paczek generowanych na turę", 
        DOMYSLNA_PACZEK_NA_TURE, 1, MAX_PACZEK);
    
    g_config.interwal_generowania = pobierz_int(
        "Interwał generowania paczek (sekundy)", 
        DOMYSLNY_INTERWAL_GENEROWANIA, 1, 3600);

    g_config.procent_ekspres = pobierz_int(
        "Procent paczek EXPRESS (%)", 
        DOMYSLNY_PROCENT_EKSPRES, 0, 100);
    
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                     CIĘŻARÓWKI                               ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    g_config.liczba_ciezarowek = pobierz_int(
        "Liczba ciężarówek", 
        DOMYSLNA_LICZBA_CIEZAROWEK, 1, MAX_CIEZAROWEK);
    
    g_config.waga_ciezarowek = pobierz_int(
        "Ładowność ciężarówki (kg)", 
        DOMYSLNA_WAGA_CIEZAROWEK, 25, 25000);
    
    g_config.pojemnosc_ciezarowek = pobierz_int(
        "Pojemność ciężarówki (m³)", 
        DOMYSLNA_POJEMNOSC_CIEZAROWEK, 1, 100);
    
    g_config.czas_rozwozu = pobierz_int(
        "Czas rozwozu (sekundy)", 
        DOMYSLNY_CZAS_ROZWOZU, 1, 6000);
    
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                       TAŚMA                                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    g_config.pojemnosc_tasmy = pobierz_int(
        "Pojemność taśmy (liczba paczek)", 
        DOMYSLNA_POJEMNOSC_TASMY, 1, MAX_POJEMNOSC_TASMY);
  
    g_config.waga_tasmy = pobierz_int(
        "Maksymalna waga na taśmie (kg)", 
        DOMYSLNA_WAGA_TASMY, 25, 10000); 
    
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                 PODSUMOWANIE KONFIGURACJI                    ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Paczek/turę: %-6d         Interwał: %-4d s                   ║\n", 
           g_config.paczek_na_ture, g_config.interwal_generowania);
        
    printf("║ Procent EXPRESS: %-3d%%                                      ║\n",g_config.procent_ekspres);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Ciężarówek: %-6d          Ładowność: %-6d kg              ║\n", 
           g_config.liczba_ciezarowek, g_config.waga_ciezarowek);
    printf("║ Pojemność: %-4d m³          Czas rozwozu: %-4d s             ║\n", 
           g_config.pojemnosc_ciezarowek, g_config.czas_rozwozu);
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Pojemność taśmy: %-4d paczek    Maks. waga: %-7d kg       ║\n", 
           g_config.pojemnosc_tasmy, g_config.waga_tasmy);
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Naciśnij Enter aby rozpocząć symulację...");
    getchar();
    printf("\n");
}

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
    
    setvbuf(stdout, NULL, _IONBF, 0);
    
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
    
    char full_msg[512];
    snprintf(full_msg, sizeof(full_msg), "%s%s%s%s", g_log_kolor, ts, msg, COL_RESET);
    write(STDOUT_FILENO, full_msg, strlen(full_msg));
    
    if (g_fd_log != -1) {
        char file_msg[512];
        snprintf(file_msg, sizeof(file_msg), "%s%s", ts, msg);
        write(g_fd_log, file_msg, strlen(file_msg));
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
    char full_msg[256];
    snprintf(full_msg, sizeof(full_msg), "%s%s", ts, msg);
    write(g_fd_sem_log, full_msg, strlen(full_msg));
}

void log_error(const char *msg) {
    char ts[32];
    log_timestamp(ts, sizeof(ts));
    const char *prefix = "[BLAD] ";
    
    if (g_semafor_log != -1) semafor_p(g_semafor_log, SEMAFOR_ZAPIS);
    char full_msg[512];
    snprintf(full_msg, sizeof(full_msg), "%s%s%s%s%s", COL_RED, ts, prefix, msg, COL_RESET);
    write(STDOUT_FILENO, full_msg, strlen(full_msg));
    
    if (g_fd_log != -1) {
        char file_msg[512];
        snprintf(file_msg, sizeof(file_msg), "%s%s%s", ts, prefix, msg);
        write(g_fd_log, file_msg, strlen(file_msg));
    }
    
    if (g_semafor_log != -1) semafor_v(g_semafor_log, SEMAFOR_ZAPIS);
}

//HANDLERY SYGNAŁÓW
void handler_sigusr1(int sig) {
    (void)sig;
    g_odjedz_niepelna = 1;
}

void handler_sigusr2(int sig) {
    (void)sig;
    g_dostarcz_ekspres = 1;
}

void handler_sigterm(int sig) {
    (void)sig;
    g_zakoncz_prace = 1;
}

void handler_sigterm_ciez(int sig) {
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
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler_sigchld;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

void ustaw_handlery_ciezarowka(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    
    sa.sa_handler = handler_sigusr1;
    sigaction(SIGUSR1, &sa, NULL);
    
    sa.sa_handler = handler_sigterm_ciez;
    sigaction(SIGTERM, &sa, NULL);
}

void ustaw_handlery_pracownik(int id) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    
    sa.sa_handler = handler_sigterm;
    sigaction(SIGTERM, &sa, NULL);
    
    if (id == 4) {
        sa.sa_handler = handler_sigusr2;
        sigaction(SIGUSR2, &sa, NULL);
    }
}

//FUNKCJE - SEMAFOR
static const char* nazwa_semafora(int nr) {
    switch(nr) {
        case SEMAFOR_TASMA: return "TASMA";
        case SEMAFOR_WOLNE_MIEJSCA: return "WOLNE_MIEJSCE";
        case SEMAFOR_PACZKI: return "PACZKI_TASMA";
        case SEMAFOR_CIEZAROWKI: return "CIEZAROWKI";
        case SEMAFOR_ZAPIS: return "ZAPIS";
        case SEMAFOR_EXPRESS: return "PACZKI_EXPRESS";
        case SEMAFOR_ID_COUNTER: return "LICZNIK ID PACZEK";
        default: return "?";
    }
}

int utworz_nowy_semafor(void) {
    int sem = semget(IPC_PRIVATE, LICZBA_SEMAFOROW, 0600 | IPC_CREAT);
    if (sem == -1) {
        perror("semget");
        return -1;
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
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "USTAW [%s] = %d\n", nazwa_semafora(nr), val);
    sem_log_write(buf);
}

int semafor_p(int sem_id, int nr) {
    if (nr != SEMAFOR_ZAPIS) {
        char buf[128];
        snprintf(buf, sizeof(buf), "P [%s] (PID %d)\n", nazwa_semafora(nr), getpid());
        sem_log_write(buf);
    }

    struct sembuf op = { .sem_num = nr, .sem_op = -1, .sem_flg = SEM_UNDO };
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) {
            if (g_zakoncz_prace || g_zakoncz_przyjmowanie || g_odjedz_niepelna) {
                return 0;  
            }
            continue;
        }
        if (errno == EIDRM || errno == EINVAL) {
            return 0;
        }
        perror("semop P");
        return 0;
    }
    return 1;
}

void semafor_v(int sem_id, int nr) {
    int current_val = semctl(sem_id, nr, GETVAL);
    if (current_val >= 30000) {
        return; 
    }
    
    struct sembuf op = { .sem_num = nr, .sem_op = 1, .sem_flg = SEM_UNDO };
    if (semop(sem_id, &op, 1) == -1) {
        if (errno == EIDRM || errno == EINVAL || errno == ERANGE) {
            return;
        }
        perror("semop V");
        return;
    }

    if (nr != SEMAFOR_ZAPIS) {
        char buf[128];
        snprintf(buf, sizeof(buf), "V [%s] (PID %d)\n", nazwa_semafora(nr), getpid());
        sem_log_write(buf);
    }
}

//GENERATORY
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
            p.waga = losuj_d(15.0, 25.0);
            break;
    }
    p.waga = round(p.waga * 1000) / 1000.0;
    return p;
}

Paczka generuj_paczke_zwykla(int id) {
    Paczka p = generuj_paczke_base(id);
    p.priorytet = ZWYKLA;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Paczka %d | Typ: ZWYKLA | Rozmiar: %s | Waga: %.3f kg\n",
             p.id, nazwa_typu(p.typ), p.waga);
    log_write(buf);
    return p;
}

Paczka generuj_paczke_ekspres(int id) {
    Paczka p = generuj_paczke_base(id);
    p.priorytet = EXPRES;
    
    char buf[256];
    snprintf(buf, sizeof(buf), "Paczka %d | Typ: EKSPRES | Rozmiar: %s | Waga: %.3f kg\n",
             p.id, nazwa_typu(p.typ), p.waga);
    log_write(buf);
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
    snprintf(buf, sizeof(buf),"\n-------------GENEROWANIE TASMY-------------\n");
    log_write(buf);
    snprintf(buf, sizeof(buf),"Maksymalna liczba paczek: %d\n", tasma->max_pojemnosc);
    log_write(buf);
    snprintf(buf, sizeof(buf),"Maksymalny udzwig: %d kg\n\n", tasma->max_waga);
    log_write(buf);
}

Ciezarowka* generuj_ciezarowke(int *liczba_ciezarowek_out) {
    int liczba_ciezarowek = g_config.liczba_ciezarowek;
    int waga_ciezarowki = g_config.waga_ciezarowek;
    int pojemnosc_ciezarowki = g_config.pojemnosc_ciezarowek;
    time_t czas_ciezarowki = g_config.czas_rozwozu;
    
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

//Kolejki komunikatow
int utworz_kolejke(void) {
    key_t klucz = ftok(".", 'Q');
    if (klucz == -1) return -1;
    return msgget(klucz, IPC_CREAT | 0600);
}

void usun_kolejke(int msgid) {
    if (msgid != -1) msgctl(msgid, IPC_RMID, NULL);
}

int wyslij_msg_ciezarowka(int msgid, pid_t pid) {
    MsgCiezarowkaPrzyTasmie msg = { .mtype = MSG_CIEZAROWKA_PRZY_TASMIE, .ciezarowka_pid = pid };
    return msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) == 0 ? 1 : 0;
}

int wyslij_msg_odpowiedz(int msgid, int wszystko, int zostalo) {
    MsgP4Odpowiedz msg = { .mtype = MSG_P4_ODPOWIEDZ, .odebrano_wszystko = wszystko, .ile_zostalo = zostalo };
    return msgsnd(msgid, &msg, sizeof(msg) - sizeof(long), 0) == 0 ? 1 : 0;
}

int odbierz_msg_ciezarowka(int msgid, MsgCiezarowkaPrzyTasmie *msg) {
    while (1) {
        if (msgrcv(msgid, msg, sizeof(*msg) - sizeof(long), MSG_CIEZAROWKA_PRZY_TASMIE, 0) != -1)
            return 1;
        if (errno == EINTR) {
            if (g_zakoncz_prace || g_zakoncz_przyjmowanie) return 0;
            continue;
        }
        return 0;
    }
}

int odbierz_msg_odpowiedz(int msgid, MsgP4Odpowiedz *msg) {
    while (1) {
        if (msgrcv(msgid, msg, sizeof(*msg) - sizeof(long), MSG_P4_ODPOWIEDZ, 0) != -1)
            return 1;
        if (errno == EINTR) {
            if (g_zakoncz_prace || g_zakoncz_przyjmowanie) return 0;
            continue;
        }
        return 0;
    }
}