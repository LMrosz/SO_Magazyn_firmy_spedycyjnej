#include "utils.h"

static int g_sem = -1;
static int g_shm_tasma = -1;
static pid_t g_p4_pid = 0;
static Tasma *g_tasma = NULL;

static void handler_dyspozytor_sigterm(int sig) {
    (void)sig;
    g_zakoncz_prace = 1;
}

static void wyswietl_menu(void) {
    printf("%s\n", COL_RED);
    printf("============================================================\n");
    printf("              DYSPOZYTOR MAGAZYNU\n");
    printf("============================================================\n");
    printf(" 1 - Ciezarowka odjedz niepelna (SIGUSR1)\n");
    printf(" 2 - P4 dostarcz paczki EXPRESS (SIGUSR2)\n");
    printf(" 3 - Zakoncz przyjmowanie (SIGTERM)\n");
    printf(" q - Wyjdz z dyspozytora\n");
    printf("============================================================\n");
    printf("Komenda> %s", COL_RESET);
    fflush(stdout);
}

static pid_t znajdz_ciezarowke(void) {
    if (g_tasma == NULL) return 0;
    semafor_p(g_sem, SEMAFOR_TASMA);
    pid_t pid = g_tasma->ciezarowka;
    semafor_v(g_sem, SEMAFOR_TASMA);
    return pid;
}

static void wyslij_sigusr1_do_ciezarowki(void) {
    char buf[128];
    pid_t pid = znajdz_ciezarowke();
    
    if (pid > 0 && kill(pid, 0) == 0) {
        if (kill(pid, SIGUSR1) == 0) {
            printf("%s[OK] Wyslano SIGUSR1 do ciezarowki PID %d%s\n", COL_GREEN, pid, COL_RESET);
            snprintf(buf, sizeof(buf), "DYSPOZYTOR: SIGUSR1 -> ciezarowka PID %d\n", pid);
            log_write(buf);
        } else {
            perror("Blad wysylania SIGUSR1");
        }
    } else {
        printf("%s[!] Brak ciezarowki przy tasmie!%s\n", COL_YELLOW, COL_RESET);
        log_write("SIGUSR1: brak ciezarowki przy tasmie\n");
    }
}

static void wyslij_sigusr2(void) {
    char buf[128];
    
    if (g_p4_pid > 0 && kill(g_p4_pid, 0) == 0) {
        if (kill(g_p4_pid, SIGUSR2) == 0) {
            printf("%s[OK] Wyslano SIGUSR2 do P4 (PID %d)%s\n", COL_GREEN, g_p4_pid, COL_RESET);
            snprintf(buf, sizeof(buf), "DYSPOZYTOR: SIGUSR2 -> P4 PID %d\n", g_p4_pid);
            log_write(buf);
        } else {
            perror("Blad wysylania SIGUSR2");
        }
    } else {
        printf("%s[!] Nie znaleziono pracownika P4!%s\n", COL_YELLOW, COL_RESET);
        log_write("SIGUSR2: nie znaleziono P4\n");
    }
}

static void wyslij_sigterm(void) {
    char buf[256];
    
    printf("%sWysylam SIGTERM do magazynu...%s\n", COL_RED, COL_RESET);
    log_write("DYSPOZYTOR: Wysylam SIGTERM do magazynu\n");

    pid_t magazyn_pid = g_tasma->magazyn_pid;
    
    if (magazyn_pid > 0 && kill(magazyn_pid, 0) == 0) {
        if (kill(magazyn_pid, SIGTERM) == 0) {
            printf("%s[OK] SIGTERM -> magazyn PID %d%s\n", COL_GREEN, magazyn_pid, COL_RESET);
            snprintf(buf, sizeof(buf), "DYSPOZYTOR: SIGTERM -> magazyn PID %d\n", magazyn_pid);
            log_write(buf);
        } else {
            perror("Blad wysylania SIGTERM");
        }
    } else {
        printf("%s[!] Proces magazynu nie istnieje!%s\n", COL_YELLOW, COL_RESET);
    }
}

int main(int argc, char *argv[]) {
    printf("%s=== DYSPOZYTOR MAGAZYNU ===%s\n", COL_RED, COL_RESET);
    
    if (argc < 6) {
        fprintf(stderr, "Uzycie: %s shmid_tasma semafor log_dir kolejka p4_pid\n", argv[0]);
        return 1;
    }
    
    g_shm_tasma = atoi(argv[1]);
    g_sem = atoi(argv[2]);
    strncpy(g_log_dir, argv[3], sizeof(g_log_dir) - 1);
    g_p4_pid = atoi(argv[5]);

    g_tasma = (Tasma *)shmat(g_shm_tasma, NULL, 0);
    if (g_tasma == (Tasma *)(-1)) {
        perror("shmat tasma");
        return 1;
    }
    
    g_tasma->dyspozytor_pid = getpid();
    
    signal(SIGTERM, handler_dyspozytor_sigterm);

    log_init(g_sem, "dyspozytor.log", COL_RED);
    sem_log_init();
    
    char buf[128];
    snprintf(buf, sizeof(buf), "DYSPOZYTOR: Uruchomiony (PID %d)\n", getpid());
    log_write(buf);

    char komenda;
    int running = 1;
    
    while (running && !g_zakoncz_prace) {
        wyswietl_menu();
        
        int result = scanf(" %c", &komenda);
        if (result == EOF) {
            printf("\nKoniec wejscia\n");
            break;
        }
        if (result != 1) continue;
        
        switch (komenda) {
            case '1':
                wyslij_sigusr1_do_ciezarowki();
                break;
            case '2':
                wyslij_sigusr2();
                break;
            case '3':
                wyslij_sigterm();
                running = 0;
                break;
            case 'q':
            case 'Q':
                log_write("DYSPOZYTOR: Zamykam\n");
                running = 0;
                break;
            default:
                printf("Nieznana komenda: %c\n", komenda);
                break;
        }
    }
    
    shmdt(g_tasma);    
    log_write("DYSPOZYTOR: Zakonczyl prace\n");
    sem_log_close(); 
    log_close();
    printf("%sDyspozytor zakonczyl.%s\n", COL_RED, COL_RESET);
    return 0;
}
