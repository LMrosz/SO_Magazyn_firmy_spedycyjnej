#include "utils.h"

int g_semafor = -1;
int g_id_tasma = -1;
Tasma *g_tasma = NULL;

void wyswietl_menu(void) {
    printf("\n\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║              DYSPOZYTOR MAGAZYNU                     ║\n");
    printf("╠══════════════════════════════════════════════════════╣\n");
    printf("║ 1 - Ciezarowka odjedz niepelna (SIGUSR1)             ║\n");
    printf("║ 2 - P4 dostarcz WSZYSTKIE ekspres (SIGUSR2)          ║\n");
    printf("║ 3 - Zakoncz przyjmowanie (SIGTERM)                   ║\n");
    printf("║     (pracownicy koncza, ciezarowki rozwoza)          ║\n");
    printf("║ s - Pokaz status                                     ║\n");
    printf("║ q - Wyjdz z dyspozytora                              ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");
    printf("Komenda> ");
    fflush(stdout);
}

void wyswietl_status(void) {
    if (g_tasma == NULL) {
        printf("Brak dostepu do tasmy!\n");
        return;
    }
    
    semafor_p(g_semafor, SEMAFOR_TASMA);
    
    printf("\n=== STATUS SYSTEMU ===\n");
    printf("Tasma: %d/%d paczek, %.2f/%d kg\n",
           g_tasma->aktualna_ilosc, g_tasma->max_pojemnosc,
           g_tasma->aktualna_waga, g_tasma->max_waga);
    
    if (g_tasma->ciezarowka > 0) {
        printf("Ciezarowka przy tasmie: PID %d\n", g_tasma->ciezarowka);
    } else {
        printf("Ciezarowka przy tasmie: BRAK\n");
    }
    
    semafor_v(g_semafor, SEMAFOR_TASMA);
    printf("======================\n");
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
    pid_t pid = znajdz_ciezarowke_przy_tasmie();
    
    if (pid > 0) {
        if (kill(pid, SIGUSR1) == 0) {
            printf("✓ Wyslano SIGUSR1 do ciezarowki PID %d - odjedzie z niepelnym ladunkiem\n", pid);
            logi("DYSPOZYTOR: Wyslano SIGUSR1 do ciezarowki PID %d\n", pid);
        } else {
            perror("Blad wysylania SIGUSR1");
        }
    } else {
        printf("✗ Brak ciezarowki przy tasmie!\n");
    }
}

void wyslij_sigusr2_do_p4(void) {
    pid_t pid = znajdz_pracownika_p4();
    
    if (pid > 0) {
        if (kill(pid, SIGUSR2) == 0) {
            printf("✓ Wyslano SIGUSR2 do P4 (PID %d) - dostarczy WSZYSTKIE paczki ekspresowe\n", pid);
            logi("DYSPOZYTOR: Wyslano SIGUSR2 do P4 PID %d\n", pid);
        } else {
            perror("Blad wysylania SIGUSR2");
        }
    } else {
        printf("✗ Nie znaleziono pracownika P4!\n");
    }
}

void wyslij_sigterm_do_wszystkich(void) {
    int wyslane = 0;
    DIR *dir = opendir("/proc");
    if (!dir) {
        perror("opendir /proc");
        return;
    }
    
    struct dirent *entry;
    
    printf("Wysylam SIGTERM:\n");
    printf("  - Pracownicy: zakoncza prace\n");
    printf("  - Generator: zatrzyma generowanie\n");
    printf("  - Ciezarowki: dokoncza rozwoz i zakoncza\n\n");
    
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
                else if (strstr(cmdline, "ciezarowki")) typ = "ciezarowka";
                else if (strstr(cmdline, "generator")) typ = "generator";
                
                printf("  SIGTERM -> PID %ld (%s)\n", pid, typ);
                wyslane++;
            }
        }
    }
    
    closedir(dir);
    
    printf("\n✓ Wyslano SIGTERM do %d procesow\n", wyslane);
    logi("DYSPOZYTOR: Wyslano SIGTERM do %d procesow (zakonczenie przyjmowania)\n", wyslane);
}

volatile sig_atomic_t g_dyspozytor_zakoncz = 0;

void handler_dyspozytor_sigterm(int sig) {
    (void)sig;
    g_dyspozytor_zakoncz = 1;
}

int main(int argc, char *argv[]) {
    printf("=== DYSPOZYTOR MAGAZYNU ===\n");
    
    if (argc < 3) {
        fprintf(stderr, "Uzycie: %s shmid_tasma semafor\n", argv[0]);
        return 1;
    }
    
    g_id_tasma = atoi(argv[1]);
    g_semafor = atoi(argv[2]);
    
    g_tasma = (Tasma *)shmat(g_id_tasma, NULL, 0);
    if (g_tasma == (Tasma *)(-1)) {
        perror("shmat tasma");
        return 1;
    }
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler_dyspozytor_sigterm;
    sigaction(SIGTERM, &sa, NULL);
    
    otworz_plik_wyniki(g_semafor);
    logi("DYSPOZYTOR: Uruchomiony (PID %d)\n", getpid());
    
    printf("Polaczono z systemem (tasma shmid=%d, semafor=%d)\n", g_id_tasma, g_semafor);
    
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
                printf("Wysylam sygnal zakonczenia przyjmowania...\n");
                wyslij_sigterm_do_wszystkich();
                break;
                
            case 's':
            case 'S':
                wyswietl_status();
                break;
                
            case 'q':
            case 'Q':
                printf("Zamykam dyspozytora...\n");
                running = 0;
                break;
                
            default:
                printf("Nieznana komenda: %c\n", komenda);
                break;
        }
    }
    
    shmdt(g_tasma);
    zamknij_plik_wyniki();
    
    printf("Dyspozytor zakonczyl prace.\n");
    return 0;
}