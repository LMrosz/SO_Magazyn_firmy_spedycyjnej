#include "utils.h"

int g_semafor = -1;
int g_id_tasma = -1;
Tasma *g_tasma = NULL;
volatile sig_atomic_t g_dyspozytor_zakoncz = 0;

void handler_dyspozytor_sigterm(int sig) {
    (void)sig;
    g_dyspozytor_zakoncz = 1;
}

void wyswietl_menu(void) {
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

pid_t znajdz_ciezarowke_przy_tasmie(void) {
    if (g_tasma == NULL) return 0;
    
    semafor_p(g_semafor, SEMAFOR_TASMA);
    pid_t pid = g_tasma->ciezarowka;
    semafor_v(g_semafor, SEMAFOR_TASMA);
    
    return pid;
}

pid_t znajdz_pracownika_p4(void) {
    DIR *dir = opendir("/proc");
    if (!dir) return 0;
    
    struct dirent *entry;
    pid_t p4_pid = 0;
    
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

void wyslij_sigusr1_do_ciezarowki(void) {
    char buf[128];
    pid_t pid = znajdz_ciezarowke_przy_tasmie();
    
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

void wyslij_sigusr2_do_p4(void) {
    char buf[128];
    pid_t pid = znajdz_pracownika_p4();
    
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
        log_write("SIGUSR2: nie znaleziono pracownika P4\n");
    }
}

void wyslij_sigterm_do_wszystkich(void) {
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
    printf("  - Generator: zatrzyma generowanie paczek\n");
    printf("  - Ciezarowki: dokoncza rozwoz i zakoncza\n\n");
    log_write("--- WYSYLANIE SIGTERM DO WSZYSTKICH ---\n");
    log_write("  - Pracownicy: zakoncza prace\n");
    log_write("  - Generator: zatrzyma generowanie paczek\n");
    log_write("  - Ciezarowki: dokoncza rozwoz i zakoncza\n\n");

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
            strstr(cmdline, "generator") != NULL) {
            if (kill((pid_t)pid, SIGTERM) == 0) {
                const char *typ = "?";
                if (strstr(cmdline, "pracownicy")) typ = "pracownik";
                else if (strstr(cmdline, "ciezarowki")) {
                    typ = "ciezarowka";
                    liczba_ciezarowek++;
                }
                else if (strstr(cmdline, "generator")) typ = "generator";
                
                printf("  SIGTERM -> PID %ld (%s)\n", pid, typ);
                wyslane++;
            }
        }
    }
    
    closedir(dir);

    semafor_v(g_semafor, SEMAFOR_P4_CZEKA);
    for (int i = 0; i < liczba_ciezarowek; i++) {
        semafor_v(g_semafor, SEMAFOR_PACZKI);
        semafor_v(g_semafor, SEMAFOR_CIEZAROWKI);
    }
    
    for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
        semafor_v(g_semafor, SEMAFOR_WOLNE_MIEJSCA);
    }
    
    printf("\n✓ Wyslano SIGTERM do %d procesow\n", wyslane);
    snprintf(buf, sizeof(buf), "DYSPOZYTOR: Wyslano SIGTERM do %d procesow (zakonczenie przyjmowania)\n", wyslane);
    log_write(buf);
}

int main(int argc, char *argv[]) {
    printf("=== DYSPOZYTOR MAGAZYNU ===\n");
    
    if (argc < 4) {
        fprintf(stderr, "Uzycie: %s shmid_tasma semafor log_dir\n", argv[0]);
        return 1;
    }
    
    g_id_tasma = atoi(argv[1]);
    g_semafor = atoi(argv[2]);
    strncpy(g_log_dir, argv[3], sizeof(g_log_dir) - 1);

    g_tasma = (Tasma *)shmat(g_id_tasma, NULL, 0);
    if (g_tasma == (Tasma *)(-1)) {
        perror("shmat tasma");
        return 1;
    }
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler_dyspozytor_sigterm;
    sigaction(SIGTERM, &sa, NULL);

    char buf[128];
    log_init(g_semafor, "dyspozytor.log", COL_RED);
    sem_log_init();
    log_write("Dyspozytor start\n");
    
    snprintf(buf, sizeof(buf),"DYSPOZYTOR: Uruchomiony (PID %d)\n", getpid());
    log_write(buf);
    snprintf(buf, sizeof(buf),"Polaczono z systemem (tasma shmid=%d, semafor=%d)\n", g_id_tasma, g_semafor);
    log_write(buf);

    char komenda;
    int running = 1;
    
    while (running && !g_dyspozytor_zakoncz) {
        wyswietl_menu();
        
        if (scanf(" %c", &komenda) != 1) {
            continue;
        }
        
        switch (komenda) {
            case '1':
                wyslij_sigusr1_do_ciezarowki();
                break;
                
            case '2':
                wyslij_sigusr2_do_p4();
                break;
                
            case '3':
                snprintf(buf, sizeof(buf),"Wysylam sygnal zakonczenia przyjmowania...\n");
                log_write(buf);
                wyslij_sigterm_do_wszystkich();
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