#include "utils.h"

int g_fd_wyniki = -1;
int g_semafor_log = -1;
char g_nazwa_pliku[MAX_NAZWA_PLIKU] = "";

volatile sig_atomic_t g_odjedz_niepelna = 0;
volatile sig_atomic_t g_dostarcz_ekspres = 0;
volatile sig_atomic_t g_zakoncz_prace = 0;
volatile sig_atomic_t g_zakoncz_przyjmowanie = 0;

//FUNKCJE POMOCNICZE - zapis na ekran i do pliku

int otworz_plik_wyniki(int semafor) {
    g_semafor_log = semafor;
    
    time_t teraz = time(NULL);
    struct tm *tm_info = localtime(&teraz);
    snprintf(g_nazwa_pliku, sizeof(g_nazwa_pliku),
             "wynik_%02d-%02d-%02d.txt",
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec);
    
    g_fd_wyniki = open(g_nazwa_pliku, O_WRONLY | O_CREAT | O_APPEND, 0644);
    
    if (g_fd_wyniki == -1) {
        perror("open plik wynikow");
        return -1;
    }
    
    return 0;
}

void zamknij_plik_wyniki(void) {
    if (g_fd_wyniki != -1) {
        close(g_fd_wyniki);
        g_fd_wyniki = -1;
    }
}

void logi(const char *format, ...) {
    char bufor[MAX_LOG_BUFOR];
    int offset = 0;
    
    time_t teraz = time(NULL);
    struct tm *tm_info = localtime(&teraz);
    offset = strftime(bufor, sizeof(bufor), "[%H:%M:%S] ", tm_info);
    
    va_list args;
    va_start(args, format);
    int len = vsnprintf(bufor + offset, sizeof(bufor) - offset, format, args);
    va_end(args);
    
    if (len < 0) return;
    
    int total_len = offset + len;
    if (total_len >= (int)sizeof(bufor)) {
        total_len = sizeof(bufor) - 1;
    }
    
    if (g_semafor_log != -1) {
        semafor_p(g_semafor_log, SEMAFOR_ZAPIS);
    }
    
    write(STDOUT_FILENO, bufor, total_len);
    
    if (g_fd_wyniki != -1) {
        write(g_fd_wyniki, bufor, total_len);
    }
    
    if (g_semafor_log != -1) {
        semafor_v(g_semafor_log, SEMAFOR_ZAPIS);
    }
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
int utworz_nowy_semafor(void) {
    key_t klucz = ftok(".", 'S');
    if (klucz == -1) {
        perror("ftok semafor");
        exit(EXIT_FAILURE);
    }
    int sem = semget(klucz, 8, 0600 | IPC_CREAT);
    if (sem == -1) {
        printf("Nie moglem utworzyc nowego semafora.\n");
        perror("semget");
        exit(EXIT_FAILURE);
    }
    
    printf("Semafor zostal utworzony : %d\n", sem);
    return sem;
}

void usun_semafor(int sem_id) {
    if (semctl(sem_id, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
    } else {
        printf("Semafor %d zostal usuniety.\n", sem_id);
    }
}

void ustaw_semafor(int sem_id, int nr, int val) {
    if (semctl(sem_id, nr, SETVAL, val) == -1) {
        perror("semctl SETVAL");
        exit(EXIT_FAILURE);
    }
    printf("Semafor %d[%d] ustawiony na %d.\n", sem_id, nr, val);
}

void semafor_p(int sem_id, int nr) {
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
    
    logi("\n-------------GENEROWANIE PACZEK POCZATKOWYCH-------------\n");
    logi("Wygenerowano %d paczek poczatkowych\n", liczba_paczek);
    logi("W tym ekspresowych: %d\n", ekspresowe);
    logi("Generowanie dynamiczne: AKTYWNE (co %d sekund, %d paczek)\n\n", 
         INTERWAL_GENEROWANIA, PACZEK_NA_TURE);
    
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
    
    logi("\n-------------GENEROWANIE TASMY-------------\n");
    logi("Maksymalna liczba paczek: %d\n", tasma->max_pojemnosc);
    logi("Maksymalny udzwig: %d kg\n\n", tasma->max_waga);
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
    
    logi("\n-------------GENEROWANIE CIEZAROWEK-------------\n");
    logi("Liczba ciezarowek: %d\n", liczba_ciezarowek);
    logi("Ladownosc: %d kg\n", waga_ciezarowki);
    logi("Pojemnosc: %d m^3\n", pojemnosc_ciezarowki);
    logi("Czas rozwozu: %ld s\n\n", czas_ciezarowki);
    
    return ciezarowki;
}