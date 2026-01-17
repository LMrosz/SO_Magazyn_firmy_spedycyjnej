#include "utils.h"

int g_sem = -1;
int g_shm_tasma = -1;
static int g_kolejka = -1;
Tasma *g_tasma = NULL;
static volatile sig_atomic_t g_koniec = 0;

static void handler_dyspozytor_sigterm(int sig) {
    (void)sig;
    g_zakoncz_prace = 1;
}

static void wyswietl_menu(void) {
    printf("\n\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║              DYSPOZYTOR MAGAZYNU                     ║\n");
    printf("╠══════════════════════════════════════════════════════╣\n");
    printf("║ 1 - Ciezarowka odjedz niepelna (SIGUSR1)             ║\n");
    printf("║ 2 - P4 dostarcz WSZYSTKIE ekspres (SIGUSR2)          ║\n");
    printf("║ 3 - Zakoncz przyjmowanie (SIGTERM)                   ║\n");
    printf("║     (pracownicy koncza, ciezarowki rozwoza)          ║\n");
    printf("║ q - Wyjdz z dyspozytora                              ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("Komenda> ");
    fflush(stdout);
}

static pid_t znajdz_ciezarowke(void) {
    if (g_tasma == NULL) return 0;
    semafor_p(g_sem, SEMAFOR_TASMA);
    pid_t pid = g_tasma->ciezarowka;
    semafor_v(g_sem, SEMAFOR_TASMA);
    return pid;
}

pid_t znajdz_p4(void) {
    DIR *dir = opendir("/proc");
    if (!dir) return 0;
    
    struct dirent *entry;
    pid_t p4_pid = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR) continue;
        
        char *end;
        long pid = strtol(entry->d_name, &end, 10);
        if (*end != '\0') continue;
        
        char path[256];
        snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
        
        FILE *f = fopen(path, "r");
        if (!f) continue;
        
        char cmdline[256] = {0};
        fread(cmdline, 1, sizeof(cmdline) - 1, f);
        fclose(f);
        
        if (strstr(cmdline, "pracownicy") != NULL) {
            char *arg = cmdline + strlen(cmdline) + 1;
            if (strcmp(arg, "4") == 0) {
                p4_pid = (pid_t)pid;
                break;
            }
        }
    }
    closedir(dir);
    return p4_pid;
}

static void wyslij_sigusr1_do_ciezarowki(void) {
    char buf[128];
    pid_t pid = znajdz_ciezarowke();
    
    if (pid > 0) {
        if (kill(pid, 0) == 0) {
            if (kill(pid, SIGUSR1) == 0) {
                printf("✓ Wyslano SIGUSR1 do ciezarowki PID %d - odjedzie z niepelnym ladunkiem\n", pid);
                snprintf(buf, sizeof(buf), "DYSPOZYTOR: Wyslano SIGUSR1 do ciezarowki PID %d\n", pid);
                log_write(buf);
            } else {
                perror("Blad wysylania SIGUSR1");
                log_write("BLAD: nie udalo sie wyslac SIGUSR1\n");
            }
        } else {
            printf("✗ Ciezarowka PID %d juz nie istnieje!\n", pid);
            snprintf(buf, sizeof(buf), "SIGUSR1: Ciezarowka PID %d juz nie istnieje\n", pid);
            log_write(buf);
        }
    } else {
        printf("✗ Brak ciezarowki przy tasmie!\n");
        log_write("SIGUSR1: brak ciezarowki przy tasmie\n");
    }
}

static void wyslij_sigusr2(void) {
    char buf[128];
    pid_t pid = znajdz_p4();
    
    if (pid > 0) {
        if (kill(pid, SIGUSR2) == 0) {
            printf("✓ Wyslano SIGUSR2 do P4 (PID %d) - dostarczy WSZYSTKIE paczki ekspresowe\n", pid);
            snprintf(buf, sizeof(buf), "DYSPOZYTOR: Wyslano SIGUSR2 do P4 PID %d\n", pid);
            log_write(buf);
        } else {
            perror("Blad wysylania SIGUSR2");
            log_write("BLAD: nie udalo sie wyslac SIGUSR2\n");
        }
    } else {
        printf("✗ Nie znaleziono pracownika P4!\n");
        printf("  (P4 mógł już zakończyć pracę lub jeszcze się nie uruchomił)\n");
        log_write("SIGUSR2: nie znaleziono pracownika P4 (zakonczyl prace lub nie uruchomiony)\n");
    }
}

void wyslij_sigterm(void) {
    char buf[256];
    int wyslane = 0;
    int liczba_ciezarowek = 0;
    DIR *dir = opendir("/proc");
    if (!dir) {
        perror("opendir /proc");
        return;
    }
    
    struct dirent *entry;
    
    printf("Wysylam SIGTERM:\n");
    printf("  - Pracownicy: zakoncza prace\n");
    printf("  - Ciezarowki: dokoncza rozwoz i zakoncza\n");
    printf("  - Magazyn: zakonczy symulacje\n\n");
    log_write("--- WYSYLANIE SIGTERM DO WSZYSTKICH ---\n");
    log_write("  - Pracownicy: zakoncza prace\n");
    log_write("  - Ciezarowki: dokoncza rozwoz i zakoncza\n");
    log_write("  - Magazyn: zakonczy symulacje\n\n");

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR) continue;
        
        char *endptr;
        long pid = strtol(entry->d_name, &endptr, 10);
        if (*endptr != '\0') continue;
        
        char path[256];
        snprintf(path, sizeof(path), "/proc/%ld/cmdline", pid);
        
        FILE *f = fopen(path, "r");
        if (!f) continue;
        
        char cmdline[256] = {0};
        fread(cmdline, 1, sizeof(cmdline) - 1, f);
        fclose(f);
        
        if (strstr(cmdline, "pracownicy") != NULL ||
            strstr(cmdline, "ciezarowki") != NULL ||
            strstr(cmdline, "generator") != NULL ||
            (strstr(cmdline, "./magazyn") != NULL && strstr(cmdline, "dyspozytor") == NULL)) {
                kill((pid_t)pid, SIGTERM);

                const char *typ = "?";
                if (strstr(cmdline, "pracownicy")) typ = "pracownik";
                else if (strstr(cmdline, "ciezarowki")) {
                    typ = "ciezarowka";
                    liczba_ciezarowek++;
                }
                else if (strstr(cmdline, "generator")) typ = "generator";
                else if (strstr(cmdline, "./magazyn")) typ = "magazyn";
                
                printf("  SIGTERM -> PID %ld (%s)\n", pid, typ);
                wyslane++;
            
        }
    }
    
    closedir(dir);

    for (int i = 0; i < liczba_ciezarowek; i++) {
        wyslij_msg_ciezarowka(g_kolejka, 0);
        wyslij_msg_odpowiedz(g_kolejka, 1, 0);
        semafor_v(g_sem, SEMAFOR_PACZKI);
        semafor_v(g_sem, SEMAFOR_CIEZAROWKI);
    }
    for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
        semafor_v(g_sem, SEMAFOR_WOLNE_MIEJSCA);
    }
    
    printf("\n✓ Wyslano SIGTERM do %d procesow\n", wyslane);
    snprintf(buf, sizeof(buf), "DYSPOZYTOR: Wyslano SIGTERM do %d procesow (zakonczenie przyjmowania)\n", wyslane);
    log_write(buf);
}

int main(int argc, char *argv[]) {
    printf("=== DYSPOZYTOR MAGAZYNU ===\n");
    
    if (argc < 5) {
        fprintf(stderr, "Uzycie: %s shmid_tasma semafor log_dir kolejka\n", argv[0]);
        return 1;
    }
    
    g_shm_tasma = atoi(argv[1]);
    g_sem = atoi(argv[2]);
    strncpy(g_log_dir, argv[3], sizeof(g_log_dir) - 1);
    g_kolejka = atoi(argv[4]);

    g_tasma = (Tasma *)shmat(g_shm_tasma, NULL, 0);
    if (g_tasma == (Tasma *)(-1)) {
        perror("shmat tasma");
        return 1;
    }
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler_dyspozytor_sigterm;
    sigaction(SIGTERM, &sa, NULL);

    char buf[128];
    log_init(g_sem, "dyspozytor.log", COL_RED);
    sem_log_init();
    log_write("Dyspozytor start\n");
    
    snprintf(buf, sizeof(buf),"DYSPOZYTOR: Uruchomiony (PID %d)\n", getpid());
    log_write(buf);
    snprintf(buf, sizeof(buf),"Polaczono z systemem (tasma shmid=%d, semafor=%d)\n", g_shm_tasma, g_sem);
    log_write(buf);

    char komenda;
    int running = 1;
    
    while (running && !g_zakoncz_prace) {
        wyswietl_menu();
        
        if (scanf(" %c", &komenda) != 1) {
            continue;
        }
        
        switch (komenda) {
            case '1':
                wyslij_sigusr1_do_ciezarowki();
                break;
                
            case '2':
                wyslij_sigusr2();
                break;
                
            case '3':
                snprintf(buf, sizeof(buf),"Wysylam sygnal zakonczenia przyjmowania...\n");
                log_write(buf);
                wyslij_sigterm();
                running = 0;
                break;
                
            case 'q':
            case 'Q':
                snprintf(buf, sizeof(buf),"Zamykam dyspozytora...\n");
                log_write(buf);
                running = 0;
                break;
                
            default:
                snprintf(buf, sizeof(buf),"Nieznana komenda: %c\n", komenda);
                log_write(buf);
                break;
        }
    }
    
    shmdt(g_tasma);    
    snprintf(buf, sizeof(buf),"Dyspozytor zakonczyl prace.\n");
    log_write(buf);
    sem_log_close(); 
    log_close();
    printf("Dyspozytor zakonczyl.\n");
    return 0;
}