#include "utils.h"

//FUNKCJE POMOCNICZE - zapis na ekran i do pliku
int g_fd_wyniki = -1;
int g_semafor_log = -1;
char g_nazwa_pliku[MAX_NAZWA_PLIKU] = "";

int otworz_plik_wyniki(int semafor){
    g_semafor_log = semafor;

    time_t teraz = time(NULL);//konwersja czasu niskopoziomowego
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

void zamknij_plik_wyniki(void){
    if (g_fd_wyniki != -1) {
        close(g_fd_wyniki);
        g_fd_wyniki = -1;
    }
}

void logi(const char *format, ...){ //uzywamy bibiotkei <stdarg.h> by nie musiec ograniczac liczby parametrow jakie funkcja przyjmuje
    char bufor[MAX_LOG_BUFOR];
    int offset = 0;
    
    time_t teraz = time(NULL);
    struct tm *tm_info = localtime(&teraz);
    offset = strftime(bufor, sizeof(bufor), "[%H:%M:%S] ", tm_info);
    
    va_list args;//uzycie bibioteki <stdarg.h>
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

int utworz_nowy_semafor(void){
    key_t klucz = ftok(".",'S');
    if(klucz == -1){
        perror("ftok semafor");
        exit(EXIT_FAILURE);
    }
    int sem=semget(klucz,6,0600|IPC_CREAT);
    if (sem==-1){
        printf("Nie moglem utworzyc nowego semafora.\n");
        perror("semget");
        exit(EXIT_FAILURE);
    }
    
    printf("Semafor zostal utworzony : %d\n",sem);
    return sem;
}

void usun_semafor(int sem_id){
     if (semctl(sem_id, 0, IPC_RMID) == -1){
        perror("semctl IPC_RMID");
    } 
    else{
        printf("Semafor %d zostal usuniety.\n", sem_id);
    }
}

void ustaw_semafor(int sem_id, int nr, int val){
    if (semctl(sem_id, nr, SETVAL, val) == -1){
        perror("semctl SETVAL");
        exit(EXIT_FAILURE);
    }
    printf("Semafor %d[%d] ustawiony na %d.\n", sem_id, nr, val);
}

void semafor_p(int sem_id, int nr){
    struct sembuf op = { .sem_num = nr, .sem_op = -1, .sem_flg = SEM_UNDO };
    while (semop(sem_id, &op, 1) == -1) {
        if (errno == EINTR) continue;
        perror("semop P");
        exit(EXIT_FAILURE);
    }
}

void semafor_v(int sem_id, int nr){
    struct sembuf op = { .sem_num = nr, .sem_op = 1, .sem_flg = SEM_UNDO };
    if (semop(sem_id, &op, 1) == -1) {
        perror("semop V");
        exit(EXIT_FAILURE);
    }
}

//GENERATORY

Paczka* generuj_paczke(int *liczba_paczek_out){
    int liczba_paczek = LICZBA_PACZEK;
    double waga_paczek = 0;
    int liczba_paczek_zwyklych = 0;

    Paczka *magazyn = (Paczka*)malloc(liczba_paczek * sizeof(Paczka));
    if (!magazyn) {
        perror("malloc magazyn");
        return NULL;
    }

    if (liczba_paczek_out) {
        *liczba_paczek_out = liczba_paczek;
    }

    int count_A, count_B, count_C;
    if (liczba_paczek >= 3) {
        count_A = losuj(1, liczba_paczek - 2);
        count_B = losuj(1, liczba_paczek - count_A - 1);
        count_C = liczba_paczek - (count_A + count_B);
    } 
    else {
        count_A = liczba_paczek;
        count_B = 0;
        count_C = 0;
    }

    int max_ekspres = (int)(liczba_paczek * 0.25);
    int ile_ekspresow = (max_ekspres > 0) ? losuj(1, max_ekspres) : 0;
    int przydzielone_ekspresy = 0;

    for (int i = 0; i < liczba_paczek; i++) {
        magazyn[i].id = i + 1;
        
        if (i < count_A) {
            magazyn[i].typ = A;
            magazyn[i].objetosc = VOL_A;
            magazyn[i].waga = losuj_d(0.1, 5.0);
        } else if (i < count_A + count_B) {
            magazyn[i].typ = B;
            magazyn[i].objetosc = VOL_B;
            magazyn[i].waga = losuj_d(5.0, 15.0);
        } else {
            magazyn[i].typ = C;
            magazyn[i].objetosc = VOL_C;
            magazyn[i].waga = losuj_d(15.0, 25.0);
        }

        magazyn[i].waga = round(magazyn[i].waga * 1000) / 1000.0;
        waga_paczek += magazyn[i].waga;

        if (przydzielone_ekspresy < ile_ekspresow && (rand() % 100 < 30)) {
            magazyn[i].priorytet = EXPRES;
            przydzielone_ekspresy++;
        } else if (przydzielone_ekspresy < ile_ekspresow && 
                   i >= liczba_paczek - (ile_ekspresow - przydzielone_ekspresy)) {
            magazyn[i].priorytet = EXPRES;
            przydzielone_ekspresy++;
        } else {
            magazyn[i].priorytet = ZWYKLA;
        }
    }

    double objetosc_paczek = count_A * VOL_A + count_B * VOL_B + count_C * VOL_C;
    
    logi("\n%-4s | %-12s | %-10s | %-10s | %-10s\n", 
           "ID", "TYP", "WAGA (kg)", "OBJ (m3)", "PRIORYTET");
    logi("-----------------------------------------------------------\n");
    
    for (int i = 0; i < liczba_paczek; i++) {
        logi("%-4d | %-12s | %-10.3f | %-10.6f | %-10s\n",
               magazyn[i].id,
               nazwa_typu(magazyn[i].typ),
               magazyn[i].waga,
               magazyn[i].objetosc,
               nazwa_priorytetu(magazyn[i].priorytet));
    }

    liczba_paczek_zwyklych = liczba_paczek - przydzielone_ekspresy;
    
    logi("\n\n-------------GENEROWANIE PACZEK-------------\n");
    logi("Wylosowano %d paczek\n", liczba_paczek);
    logi("Liczba paczek A: %d\n", count_A);
    logi("Liczba paczek B: %d\n", count_B);
    logi("Liczba paczek C: %d\n\n", count_C);
    logi("Paczki ekspresowe: %d\n", przydzielone_ekspresy);
    logi("Paczki zwykle: %d\n", liczba_paczek_zwyklych);
    logi("Waga paczek: %.3f kg\n", waga_paczek);
    logi("Objetosc paczek: %f m3\n\n", objetosc_paczek);

    return magazyn;
}

void generuj_tasme(Tasma* tasma) {
    tasma->head = 0;
    tasma->tail = 0;
    tasma->aktualna_ilosc = 0;
    tasma->aktualna_waga = 0;
    tasma->max_pojemnosc = POJEMNOSC_TASMY;
    tasma->max_waga = WAGA_TASMY;

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


