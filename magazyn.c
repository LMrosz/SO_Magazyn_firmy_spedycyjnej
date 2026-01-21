#include "utils.h"

static int g_shm_tasma = -1;
static int g_sem = -1;
static int g_shm_okienko = -1;
static int g_kolejka = -1;
static int g_shm_licznik = -1;
static Tasma *g_tasma = NULL;
static OkienkoEkspresShm *g_okienko = NULL;
static LicznikId *g_licznik = NULL;

static pid_t g_pracownicy_pids[LICZBA_PRACOWNIKOW];
static pid_t *g_ciezarowki_pids = NULL;
static pid_t g_dyspozytor_pid = 0;
static int g_liczba_ciezarowek = 0;
static volatile sig_atomic_t g_procesy_odblokowane = 0;

void odblokuj_procesy(int liczba_ciezarowek);

void cleanup(void) {
    log_write("CLEANUP: Sprzatanie zasobow...\n");
    
    if (g_tasma && g_tasma != (void*)-1) shmdt(g_tasma);
    if (g_okienko && g_okienko != (void*)-1) shmdt(g_okienko);
    if (g_licznik && g_licznik != (void*)-1) shmdt(g_licznik);
    if (g_shm_tasma != -1) shmctl(g_shm_tasma, IPC_RMID, NULL);
    if (g_shm_okienko != -1) shmctl(g_shm_okienko, IPC_RMID, NULL);
    if (g_shm_licznik != -1) shmctl(g_shm_licznik, IPC_RMID, NULL);
    if (g_kolejka != -1) usun_kolejke(g_kolejka);
    if (g_ciezarowki_pids) free(g_ciezarowki_pids);
    
    log_write("CLEANUP: Zakonczone\n");
    log_close();
    
    if (g_sem != -1) usun_semafor(g_sem);
}

void handle_sigint(int sig) {
    (void)sig;
    g_zakoncz_prace = 1;
}

void odblokuj_procesy(int liczba_ciezarowek) {
    if (g_procesy_odblokowane) return;
    g_procesy_odblokowane = 1;
    
    for (int i = 0; i < liczba_ciezarowek; i++) {
        semafor_v(g_sem, SEMAFOR_PACZKI);
        semafor_v(g_sem, SEMAFOR_CIEZAROWKI);
    }

    for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
        semafor_v(g_sem, SEMAFOR_WOLNE_MIEJSCA);
    }
}

pid_t uruchom_dyspozytora(int shm_tasma, int semafor_id, int kolejka_p4, pid_t p4_pid) {
    char dyspozytor_cmd[512];
    sprintf(dyspozytor_cmd, "./dyspozytor %d %d %s %d %d", 
            shm_tasma, semafor_id, g_log_dir, kolejka_p4, p4_pid);
    
    char *tmux_env = getenv("TMUX");
    if (tmux_env != NULL) {
        char tmux_cmd[1024];
        sprintf(tmux_cmd, "tmux split-window -h -p 40 '%s'", dyspozytor_cmd);
        int result = system(tmux_cmd);
        if (result == 0) {
            log_write("Dyspozytor uruchomiony w panelu tmux\n");
            
            pid_t pid = 0;
            for (int i = 0; i < 20 && pid == 0; i++) {
                // usleep(100000);
                pid = g_tasma->dyspozytor_pid;
            }
            return pid;
        }
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        char arg1[32], arg2[32], arg3[32], arg4[32];
        sprintf(arg1, "%d", shm_tasma);
        sprintf(arg2, "%d", g_sem);
        sprintf(arg3, "%d", g_kolejka);
        sprintf(arg4, "%d", p4_pid);
        execl("./dyspozytor", "dyspozytor", arg1, arg2, g_log_dir, arg3, arg4, NULL);
        perror("execl dyspozytor");
        exit(1);
    }
    return pid;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    srand(time(NULL));
    konfiguruj_symulacje();

    for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
        g_pracownicy_pids[i] = 0;
    }

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    ustaw_handler_sigchld();

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    snprintf(g_log_dir, sizeof(g_log_dir), "logi_%02d-%02d-%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
    mkdir(g_log_dir, 0755);

    g_sem = utworz_nowy_semafor();
    ustaw_semafor(g_sem, SEMAFOR_TASMA, 1);
    ustaw_semafor(g_sem, SEMAFOR_PACZKI, 0);
    ustaw_semafor(g_sem, SEMAFOR_CIEZAROWKI, 1);
    ustaw_semafor(g_sem, SEMAFOR_ZAPIS, 1);
    ustaw_semafor(g_sem, SEMAFOR_EXPRESS, 1);
    ustaw_semafor(g_sem, SEMAFOR_ID_COUNTER, 1);
    ustaw_semafor(g_sem, SEMAFOR_WAGA_DOSTEPNA, 0);

    g_kolejka = utworz_kolejke();
    if (g_kolejka == -1) {
        log_error("Blad tworzenia kolejki\n");
        cleanup();
        return 1;
    }

    log_init(g_sem, "magazyn.log", COL_GREEN);
    sem_log_init();
    log_write("=== SYMULACJA MAGAZYNU ===\n");
    
    char buf[512];

    Ciezarowka *ciezarowki = generuj_ciezarowke(&g_liczba_ciezarowek);
    if (!ciezarowki) {
        cleanup();
        return 1;
    }
    
    g_ciezarowki_pids = malloc(g_liczba_ciezarowek * sizeof(pid_t));
    if (!g_ciezarowki_pids) {
        free(ciezarowki);
        cleanup();
        return 1;
    }
    for (int i = 0; i < g_liczba_ciezarowek; i++) {
        g_ciezarowki_pids[i] = 0;
    }

    g_shm_tasma = shmget(IPC_PRIVATE, sizeof(Tasma), IPC_CREAT | 0600);
    g_tasma = (Tasma *)shmat(g_shm_tasma, NULL, 0);
    
    g_shm_okienko = shmget(IPC_PRIVATE, sizeof(OkienkoEkspresShm), IPC_CREAT | 0600);
    g_okienko = (OkienkoEkspresShm *)shmat(g_shm_okienko, NULL, 0);
    g_okienko->ilosc = 0;
    g_okienko->gotowe = 0;
    
    g_shm_licznik = shmget(IPC_PRIVATE, sizeof(LicznikId), IPC_CREAT | 0600);
    g_licznik = (LicznikId *)shmat(g_shm_licznik, NULL, 0);
    g_licznik->nastepne_id = 1;
    g_licznik->generowanie_aktywne = 1;

    generuj_tasme(g_tasma);
    g_tasma->magazyn_pid = getpid();
    g_tasma->dyspozytor_pid = 0;
    ustaw_semafor(g_sem, SEMAFOR_WOLNE_MIEJSCA, g_tasma->max_pojemnosc);

    char arg_id[16], arg_sem[16], arg_tasma[16], arg_okienko[16];
    char arg_kolejka[16], arg_paczki[16], arg_interwal[16], arg_licznik[16];
    char arg_waga[16], arg_poj[16], arg_czas[16];
    
    sprintf(arg_sem, "%d", g_sem);
    sprintf(arg_tasma, "%d", g_shm_tasma);
    sprintf(arg_okienko, "%d", g_shm_okienko);
    sprintf(arg_kolejka, "%d", g_kolejka);
    sprintf(arg_interwal, "%d", g_config.interwal_generowania);
    sprintf(arg_licznik, "%d", g_shm_licznik);

    for (int i = 0; i < 3; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            sprintf(arg_id, "%d", i + 1);
            sprintf(arg_paczki, "%d", g_config.paczek_na_ture);
            execl("./pracownicy", "pracownicy", arg_id, arg_sem, arg_tasma, arg_okienko,
                  g_log_dir, arg_kolejka, arg_paczki, arg_interwal, arg_licznik, NULL);
            perror("execl pracownicy");
            exit(1);
        } else if (pid > 0) {
            g_pracownicy_pids[i] = pid;
            snprintf(buf, sizeof(buf), "Uruchomiono P%d (PID %d)\n", i + 1, pid);
            log_write(buf);
        }
    }
    {
        pid_t pid = fork();
        if (pid == 0) {
            sprintf(arg_id, "4");
            sprintf(arg_paczki, "%d", g_config.paczek_express);
            execl("./pracownik4", "pracownik4", arg_id, arg_sem, arg_tasma, arg_okienko,
                  g_log_dir, arg_kolejka, arg_paczki, arg_interwal, arg_licznik, NULL);
            perror("execl pracownik4");
            exit(1);
        } else if (pid > 0) {
            g_pracownicy_pids[3] = pid;
            snprintf(buf, sizeof(buf), "Uruchomiono P4 (PID %d) - EXPRESS\n", pid);
            log_write(buf);
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
            exit(1);
        } else if (pid > 0) {
            g_ciezarowki_pids[i] = pid;
        }
    }

    snprintf(buf, sizeof(buf), "Uruchomiono %d ciezarowek\n", g_liczba_ciezarowek);
    log_write(buf);

    g_dyspozytor_pid = uruchom_dyspozytora(g_shm_tasma, g_sem, g_kolejka, g_pracownicy_pids[3]);

    log_write("Symulacja uruchomiona. Czekam na sygnal 3 od dyspozytora...\n");
    
    while (!g_zakoncz_prace) {
        pause();
    }
    log_write("Otrzymano sygnal 3 - Zamykanie symulacji...\n");
    if (g_licznik != NULL) {
        g_licznik->generowanie_aktywne = 0;
    }

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
    
    log_write("Czekam na zakonczenie pracownikow...\n");
    int retries = 100;
    int pracownicy_aktywni = 1;
    while (pracownicy_aktywni && retries > 0) {
        pracownicy_aktywni = 0;
        for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
            if (g_pracownicy_pids[i] > 0) {
                int status;
                pid_t result = waitpid(g_pracownicy_pids[i], &status, WNOHANG);
                if (result == g_pracownicy_pids[i] || result == -1) {
                    g_pracownicy_pids[i] = 0;
                } else {
                    pracownicy_aktywni = 1;
                }
            }
        }
        if (pracownicy_aktywni) {
            retries--;
        }
    }
    for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
        if (g_pracownicy_pids[i] > 0) {
            kill(g_pracownicy_pids[i], SIGKILL);
            waitpid(g_pracownicy_pids[i], NULL, 0);
            g_pracownicy_pids[i] = 0;
        }
    }
    log_write("Wszyscy pracownicy zakonczone.\n");
    
    log_write("Czekam az ciezarowki rozwioza przesylki i wroca...\n");
    for (int i = 0; i < g_liczba_ciezarowek; i++) {
        if (g_ciezarowki_pids[i] > 0) {
            waitpid(g_ciezarowki_pids[i], NULL, 0);
            g_ciezarowki_pids[i] = 0;
        }
    }
    log_write("Wszystkie ciezarowki wrocily do magazynu.\n");
    
    log_write("Wszyscy pracownicy i ciezarowki zakonczone.\n");
    free(ciezarowki);
    
    if (g_dyspozytor_pid > 0) {
        for (int i = 0; i < 30; i++) {
            int status;
            pid_t result = waitpid(g_dyspozytor_pid, &status, WNOHANG);
            if (result == g_dyspozytor_pid || result == -1) break;
            // usleep(100000);
        }
    }
    
    log_write("=== SYMULACJA ZAKONCZONA ===\n");
    cleanup();
    return 0;
}
