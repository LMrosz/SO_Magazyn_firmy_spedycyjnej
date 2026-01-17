#include "utils.h"

int g_shm_tasma = -1;
int g_sem = -1;
int g_shm_okienko = -1;
int g_kolejka =-1;
int g_shm_licznik =-1;
Tasma *g_tasma = NULL;
OkienkoEkspresShm *g_okienko = NULL;
LicznikId *g_licznik = NULL;

pid_t g_pracownicy_pids[LICZBA_PRACOWNIKOW];
pid_t *g_ciezarowki_pids = NULL;
pid_t g_dyspozytor_pid = 0;
int g_liczba_ciezarowek = 0;
static volatile sig_atomic_t g_procesy_odblokowane = 0;

void cleanup(void) {
    log_write("CLEANUP: Rozpoczynam sprzatanie zasobow...\n");
    
    // 1. Najpierw odłączamy pamięć dzieloną
    if (g_tasma && g_tasma != (void*)-1) {
        char buf[256];
        snprintf(buf, sizeof(buf), 
            "CLEANUP: Odlaczam tasme (zostalo: %d paczek, %.2fkg)\n",
            g_tasma->aktualna_ilosc, g_tasma->aktualna_waga);
        log_write(buf);
        shmdt(g_tasma);
    }
    if (g_okienko && g_okienko != (void*)-1) {
        shmdt(g_okienko);
    }
    if (g_licznik && g_licznik != (void*)-1) {
        char buf[128];
        snprintf(buf, sizeof(buf), "CLEANUP: Odlaczam licznik paczek (ostatnie ID: %d)\n", g_licznik->nastepne_id - 1);
        log_write(buf);
        shmdt(g_licznik);
    }
    
    if (g_shm_tasma != -1) {
        log_write("CLEANUP: Usuwam pamiec dzielona tasmy\n");
        shmctl(g_shm_tasma, IPC_RMID, NULL);
    }
    if (g_shm_okienko != -1) {
        log_write("CLEANUP: Usuwam pamiec dzielona okienka\n");
        shmctl(g_shm_okienko, IPC_RMID, NULL);
    }
    if (g_shm_licznik != -1) {
        log_write("CLEANUP: Usuwam pamiec dzielona licznika\n");
        shmctl(g_shm_licznik, IPC_RMID, NULL);
    }
    
    if (g_kolejka != -1) {
        log_write("CLEANUP: Usuwam kolejke komunikatow\n");
        usun_kolejke(g_kolejka);
    }
    
    if (g_ciezarowki_pids) {
        log_write("CLEANUP: Zwalniam pamiec tablicy PID ciezarowek\n");
        free(g_ciezarowki_pids);
    }
    
    log_write("CLEANUP: Sprzatanie zakonczone\n");
    log_close();
    
    if (g_sem != -1) {
        usun_semafor(g_sem);
    }
}

void handle_sigint(int sig) {
    (void)sig;
    printf("\n\nOtrzymano SIGINT - czyszczenie zasobow...\n");
    g_zakoncz_prace = 1;

    if (g_licznik != NULL){
        g_licznik->generowanie_aktywne = 0;
    }

    for (int i=0; i<LICZBA_PRACOWNIKOW; i++){
        if(g_pracownicy_pids[i]>0){
            kill(g_pracownicy_pids[i], SIGTERM);
        }
    }

    for (int i=0; i<g_liczba_ciezarowek;i++){
        if(g_ciezarowki_pids != NULL && g_ciezarowki_pids[i]>0){
            kill(g_ciezarowki_pids[i], SIGTERM);
        }
    }

    if(g_dyspozytor_pid>0){
        kill(g_dyspozytor_pid, SIGTERM);
    }
}

void odblokuj_procesy(int liczba_ciezarowek){
    if (g_procesy_odblokowane) return;
    g_procesy_odblokowane = 1;
    
    for(int i=0;i<liczba_ciezarowek;i++){
        semafor_v(g_sem,SEMAFOR_PACZKI);
        semafor_v(g_sem,SEMAFOR_CIEZAROWKI);
        wyslij_msg_ciezarowka(g_kolejka, 0);
        wyslij_msg_odpowiedz(g_kolejka, 1, 0);
    }

    for(int i=0;i<LICZBA_PRACOWNIKOW;i++){
        semafor_v(g_sem, SEMAFOR_WOLNE_MIEJSCA);
    }
}

pid_t uruchom_dyspozytora(int shm_tasma, int semafor_id, int kolejka_p4) {
    char dyspozytor_cmd[512];
    sprintf(dyspozytor_cmd, "./dyspozytor %d %d %s %d", shm_tasma, semafor_id,g_log_dir, kolejka_p4);
    
    char *tmux_env = getenv("TMUX");
    if (tmux_env != NULL) {
        char tmux_cmd[1024];
        sprintf(tmux_cmd, "tmux split-window -h -p 40 '%s; read -p \"Nacisnij Enter...\"'", dyspozytor_cmd);
        int result = system(tmux_cmd);
        if (result == 0) {
            log_write("Dyspozytor uruchomiony w nowym panelu tmux\n");
            // usleep(500000);
            
            FILE *fp = popen("pgrep -n -f 'dyspozytor'", "r");
            if (fp) {
                char pid_str[32] = {0};
                if (fgets(pid_str, sizeof(pid_str), fp)) {
                    pid_t pid = atoi(pid_str);
                    pclose(fp);
                    char buf[128];
                    snprintf(buf, sizeof(buf), "Dyspozytor PID = %d\n", pid);
                    log_write(buf);
                    return pid;
                }
                pclose(fp);
            }
            return -1;
        }
        log_write("Blad tmux, uruchamiam normalnie...\n");
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        char arg1[32], arg2[32], arg3[32];
        sprintf(arg1, "%d", shm_tasma);
        sprintf(arg2, "%d", g_sem);
        sprintf(arg3, "%d", g_kolejka);
        execl("./dyspozytor", "dyspozytor", arg1, arg2, g_log_dir, arg3, NULL);
        perror("execl dyspozytor");
        exit(1);
    } else if (pid > 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Uruchomiono dyspozytora PID = %d (w tym samym terminalu)\n", pid);
        log_write(buf);
    } else {
        perror("fork dyspozytor");
    }
    
    return pid;
}

int main() {
    srand(time(NULL));
    konfiguruj_symulacje();

    for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
        g_pracownicy_pids[i] = 0;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    ustaw_handler_sigchld();

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    snprintf(g_log_dir, sizeof(g_log_dir), "logi_%02d-%02d-%02d",
             tm->tm_hour, tm->tm_min, tm->tm_sec);
    mkdir(g_log_dir, 0755);

    g_sem = utworz_nowy_semafor();
    ustaw_semafor(g_sem, SEMAFOR_TASMA, 1);
    ustaw_semafor(g_sem, SEMAFOR_PACZKI, 0);
    ustaw_semafor(g_sem, SEMAFOR_CIEZAROWKI, 1);
    ustaw_semafor(g_sem, SEMAFOR_ZAPIS, 1);
    ustaw_semafor(g_sem, SEMAFOR_EXPRESS, 1);
    ustaw_semafor(g_sem, SEMAFOR_ID_COUNTER, 1);

    g_kolejka = utworz_kolejke();
    if (g_kolejka == -1) {
        log_error("Blad tworzenia kolejki P4\n");
        cleanup();
        return 1;
    }

    log_init(g_sem, "magazyn.log", COL_GREEN);
    sem_log_init();
    log_write("=== SYMULACJA MAGAZYNU ===\n");
    char buf[512];
    snprintf(buf, sizeof(buf), 
        "Konfiguracja: paczek/ture=%d, interwal=%ds, ekspres=%d%%\n",
        g_config.paczek_na_ture, g_config.interwal_generowania, g_config.procent_ekspres);
    log_write(buf);
    snprintf(buf, sizeof(buf), 
        "Ciezarowki: N=%d, W=%dkg, V=%dm3, Ti=%ds\n",
        g_config.liczba_ciezarowek, g_config.waga_ciezarowek,
        g_config.pojemnosc_ciezarowek, g_config.czas_rozwozu);
    log_write(buf);
    snprintf(buf, sizeof(buf), 
        "Tasma: K=%d paczek, M=%dkg\n",
        g_config.pojemnosc_tasmy, g_config.waga_tasmy);
    log_write(buf);

    Ciezarowka *ciezarowki = generuj_ciezarowke(&g_liczba_ciezarowek);
    if (!ciezarowki) {
        log_error("Nie udalo sie zaalokowac pamieci dla ciezarowek\n");
        cleanup();
        return 1;
    }
    g_ciezarowki_pids = malloc(g_liczba_ciezarowek * sizeof(pid_t));
    if (!g_ciezarowki_pids) {
        log_error("Nie udalo sie zaalokowac tablicy PID ciezarowek\n");
        free(ciezarowki);
        cleanup();
        return 1;
    }
    for (int i = 0; i < g_liczba_ciezarowek; i++) {
        g_ciezarowki_pids[i] = 0;
    }

    g_shm_tasma = shmget(IPC_PRIVATE, sizeof(Tasma), IPC_CREAT | 0600);
    if (g_shm_tasma == -1) {
        log_error("Nie udalo sie utworzyc pamieci dzielonej dla tasmy\n");
        free(ciezarowki);
        cleanup();
        return 1;
    }

    g_tasma = (Tasma *)shmat(g_shm_tasma, NULL, 0);
    if (g_tasma == (Tasma *)(-1)) {
        log_error("Nie udalo sie utworzyc pamieci dzielonej dla tasmy\n");
        free(ciezarowki);
        cleanup();
        return 1;
    }

    g_shm_okienko = shmget(IPC_PRIVATE, sizeof(OkienkoEkspresShm), IPC_CREAT | 0600);
    if (g_shm_okienko == -1) {
        log_error("Nie udalo sie utworzyc pamieci dzielonej dla okienka\n");
        free(ciezarowki);
        cleanup();
        return 1;
    }

    g_okienko = (OkienkoEkspresShm *)shmat(g_shm_okienko, NULL, 0);
    if (g_okienko == (OkienkoEkspresShm *)(-1)) {
        log_error("Nie udalo sie utworzyc pamieci dzielonej dla okienka\n");
        free(ciezarowki);
        cleanup();
        return 1;
    }

    g_okienko->ilosc = 0;
    g_okienko->ciezarowka_pid = 0;
    g_okienko->gotowe = 0;

    g_shm_licznik = shmget(IPC_PRIVATE, sizeof(LicznikId), IPC_CREAT | 0600);
    if (g_shm_licznik == -1) {
        log_error("Nie udalo sie utworzyc pamieci dzielonej dla licznika paczel\n");
        free(ciezarowki);
        cleanup();
        return 1;
    }
    
    g_licznik = (LicznikId *)shmat(g_shm_licznik, NULL, 0);
    if (g_licznik == (LicznikId *)(-1)) {
        log_error("Nie udalo sie utworzyc pamieci dzielonej dla licznika\n");
        free(ciezarowki);
        cleanup();
        return 1;
    }

    g_licznik->nastepne_id = 1;
    g_licznik->generowanie_aktywne = (g_config.paczek_na_ture > 0) ? 1 : 0;

    snprintf(buf, sizeof(buf), "Pamiec dzielona: tasma=%d, ekspresy=%d, licznik=%d\n",
             g_shm_tasma, g_shm_okienko, g_shm_licznik);
    log_write(buf);
    
    generuj_tasme(g_tasma);
    ustaw_semafor(g_sem, SEMAFOR_WOLNE_MIEJSCA, g_tasma->max_pojemnosc);
    snprintf(buf, sizeof(buf), "Zainicjalizowano tasme: K=%d, M=%dkg, semafor WOLNE_MIEJSCA=%d\n",
        g_tasma->max_pojemnosc, g_tasma->max_waga, g_tasma->max_pojemnosc);
    log_write(buf);

    char arg_id[16], arg_sem[16], arg_tasma[16], arg_okienko[16];
    char arg_kolejka[16], arg_paczki[16], arg_interwal[16], arg_licznik[16];
    char arg_waga[16], arg_poj[16], arg_czas[16], arg_procent[16];
    
    sprintf(arg_sem, "%d", g_sem);
    sprintf(arg_tasma, "%d", g_shm_tasma);
    sprintf(arg_okienko, "%d", g_shm_okienko);
    sprintf(arg_kolejka, "%d", g_kolejka);
    sprintf(arg_paczki, "%d", g_config.paczek_na_ture);
    sprintf(arg_interwal, "%d", g_config.interwal_generowania);
    sprintf(arg_licznik, "%d", g_shm_licznik);
    sprintf(arg_procent, "%d", g_config.procent_ekspres);

    for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            sprintf(arg_id, "%d", i + 1);
            execl("./pracownicy", "pracownicy", arg_id, arg_sem, arg_tasma, arg_okienko,
                  g_log_dir, arg_kolejka, arg_paczki, arg_interwal, arg_licznik, arg_procent, NULL);
            exit(0);
        } else if (pid > 0) {
            g_pracownicy_pids[i] = pid;
            snprintf(buf, sizeof(buf), "Uruchomiono P%d (PID %d) - %s\n", 
                     i + 1, pid, (i + 1 == 4) ? "EKSPRES" : "ZWYKLE");
            log_write(buf);
        } else {
            snprintf(buf, sizeof(buf), "Blad fork() dla P%d\n", i + 1);
            log_error(buf);
        }
    }
    
    for (int i = 0; i < g_liczba_ciezarowek; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            sprintf(arg_id, "%d", ciezarowki[i].id_ciezarowki);
            sprintf(arg_waga, "%d", ciezarowki[i].waga_ciezarowki);
            sprintf(arg_poj, "%d", ciezarowki[i].pojemnosc_ciezarowki);
            sprintf(arg_czas, "%ld", ciezarowki[i].czas_rozwozu);
            execl("./ciezarowki", "ciezarowki", arg_id, arg_tasma, arg_sem,
                  arg_waga, arg_poj, arg_czas, arg_okienko, g_log_dir, arg_kolejka, NULL);
            exit(0);
        } else if (pid > 0) {
            g_ciezarowki_pids[i] = pid;
        } else {
            snprintf(buf, sizeof(buf), "Blad fork() dla ciezarowki %d\n", i + 1);
            log_error(buf);
        }
    }

    snprintf(buf, sizeof(buf), "Uruchomiono %d ciezarowek\n", g_liczba_ciezarowek);
    log_write(buf);

    g_dyspozytor_pid = uruchom_dyspozytora(g_shm_tasma, g_sem, g_kolejka);

    if (g_dyspozytor_pid > 0) {
        snprintf(buf, sizeof(buf), "Uruchomiono dyspozytora (PID %d)\n", g_dyspozytor_pid);
        log_write(buf);
    } else if (g_dyspozytor_pid == -1) {
        log_write("Dyspozytor uruchomiony w osobnym panelu tmux\n");
    }

    int pracownicy = LICZBA_PRACOWNIKOW;
    int ciezarowki_aktywne = g_liczba_ciezarowek;

    while ((pracownicy > 0 || ciezarowki_aktywne > 0) && !g_zakoncz_prace) {
        for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
            if (g_pracownicy_pids[i] > 0) {
                int status;
                pid_t result = waitpid(g_pracownicy_pids[i], &status, WNOHANG);
                if (result > 0) {
                    snprintf(buf, sizeof(buf), "Pracownik P%d zakonczyl swoja prace\n", i + 1);
                    log_write(buf);
                    g_pracownicy_pids[i] = 0;
                    pracownicy--;
                } else if (result == -1 && errno == ECHILD) {
                    g_pracownicy_pids[i] = 0;
                    pracownicy--;
                }
            }
        }

        for (int i = 0; i < g_liczba_ciezarowek; i++) {
            if (g_ciezarowki_pids[i] > 0) {
                int status;
                pid_t result = waitpid(g_ciezarowki_pids[i], &status, WNOHANG);
                if (result > 0) {
                    snprintf(buf, sizeof(buf), "Ciezarowka %d zakonczyla prace\n", i + 1);
                    log_write(buf);
                    g_ciezarowki_pids[i] = 0;
                    ciezarowki_aktywne--;
                } else if (result == -1 && errno == ECHILD) {
                    g_ciezarowki_pids[i] = 0;
                    ciezarowki_aktywne--;
                }
            }
        }
        
        if (pracownicy == 0 && ciezarowki_aktywne > 0) {
            semafor_p(g_sem, SEMAFOR_TASMA);
            int paczki_na_tasmie = g_tasma->aktualna_ilosc;
            semafor_v(g_sem, SEMAFOR_TASMA);
            
            if (paczki_na_tasmie == 0 && !g_licznik->generowanie_aktywne) {
                log_write("Wykryto brak pracy - koncze symulacje.\n");
                g_zakoncz_prace = 1;
                for (int i = 0; i < g_liczba_ciezarowek; i++) {
                    if (g_ciezarowki_pids[i] > 0) {
                        kill(g_ciezarowki_pids[i], SIGTERM);
                    }
                    semafor_v(g_sem, SEMAFOR_PACZKI);
                    semafor_v(g_sem, SEMAFOR_CIEZAROWKI);
                }
            }
        }
        
        // usleep(100000);
    }

    if (g_zakoncz_prace) {
        log_write("Przerwano - zamykanie...\n");
        
        for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
            if (g_pracownicy_pids[i] > 0) {
                kill(g_pracownicy_pids[i], SIGTERM);
            }
        }
        for (int i = 0; i < g_liczba_ciezarowek; i++) {
            if (g_ciezarowki_pids[i] > 0) {
                kill(g_ciezarowki_pids[i], SIGTERM);
            }
        }
        
        odblokuj_procesy(g_liczba_ciezarowek);
        
        for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
            if (g_pracownicy_pids[i] > 0) {
                waitpid(g_pracownicy_pids[i], NULL, 0);
            }
        }
        for (int i = 0; i < g_liczba_ciezarowek; i++) {
            if (g_ciezarowki_pids[i] > 0) {
                waitpid(g_ciezarowki_pids[i], NULL, 0);
            }
        }
    }
    
    log_write("Wszyscy pracownicy i ciezarowki zakonczone.\n");
    
    free(ciezarowki);
    
    if (g_dyspozytor_pid > 0) {
        kill(g_dyspozytor_pid, SIGTERM);
        waitpid(g_dyspozytor_pid, NULL, 0);
        log_write("Dyspozytor zakonczyl prace\n");
    } else if (g_dyspozytor_pid == -1) {
        system("pkill -f './dyspozytor'");
        // sleep(1);
        log_write("Dyspozytor (tmux) zakonczyl prace\n");
    }
    
    log_write("\n\n\n Symulacja zakonczyla sie poprawnie\n");
    cleanup();
    return 0;
}