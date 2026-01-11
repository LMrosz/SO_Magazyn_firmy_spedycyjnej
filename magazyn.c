#include "utils.h"

int g_id_magazyn = -1;
int g_id_tasma = -1;
int g_semafor = -1;
int g_id_okienko = -1;
Magazyn_wspolny *g_wspolny = NULL;
Tasma *g_tasma = NULL;
OkienkoEkspresShm *g_okienko = NULL;

void cleanup(void) {
    if (g_wspolny != NULL && g_wspolny != (Magazyn_wspolny *)(-1)) {
        shmdt(g_wspolny);
        g_wspolny = NULL;
    }
    
    if (g_tasma != NULL && g_tasma != (Tasma *)(-1)) {
        shmdt(g_tasma);
        g_tasma = NULL;
    }
    
    if (g_okienko != NULL && g_okienko != (OkienkoEkspresShm *)(-1)) {
        shmdt(g_okienko);
        g_okienko = NULL;
    }
    
    if (g_id_magazyn != -1) {
        shmctl(g_id_magazyn, IPC_RMID, NULL);
        printf("Usunieto pamiec dzielona magazynu: %d\n", g_id_magazyn);
        g_id_magazyn = -1;
    }
    
    if (g_id_tasma != -1) {
        shmctl(g_id_tasma, IPC_RMID, NULL);
        printf("Usunieto pamiec dzielona tasmy: %d\n", g_id_tasma);
        g_id_tasma = -1;
    }
    
    if (g_semafor != -1) {
        usun_semafor(g_semafor);
        g_semafor = -1;
    }
    
    if (g_id_okienko != -1) {
        shmctl(g_id_okienko, IPC_RMID, NULL);
        g_id_okienko = -1;
    }
    
    log_close();
}

void handle_sigint(int sig) {
    (void)sig;
    printf("\n\nOtrzymano SIGINT - czyszczenie zasobow...\n");
    g_zakoncz_prace = 1;
}

pid_t uruchom_dyspozytora(int shmid_tasma, int semafor_id) {
    char dyspozytor_cmd[256];
    sprintf(dyspozytor_cmd, "./dyspozytor %d %d %s", shmid_tasma, semafor_id,g_log_dir);
    
    char *tmux_env = getenv("TMUX");
    if (tmux_env != NULL) {
        char tmux_cmd[512];
        sprintf(tmux_cmd, "tmux split-window -h -p 40 '%s; read -p \"Nacisnij Enter...\"'", dyspozytor_cmd);
        int ret = system(tmux_cmd);
        if (ret == 0) {
            log_write("Dyspozytor uruchomiony w nowym panelu tmux\n");
            usleep(500000);
            
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
        char arg1[32], arg2[32];
        sprintf(arg1, "%d", shmid_tasma);
        sprintf(arg2, "%d", semafor_id);
        execl("./dyspozytor", "dyspozytor", arg1, arg2, g_log_dir, NULL);
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

void odblokuj_ciezarowki(int liczba_ciezarowek) {
    for (int i = 0; i < liczba_ciezarowek; i++) {
        semafor_v(g_semafor, SEMAFOR_PACZKI);
    }
    for (int i = 0; i < liczba_ciezarowek; i++) {
        semafor_v(g_semafor, SEMAFOR_CIEZAROWKI);
    }

    semafor_v(g_semafor, SEMAFOR_P4_CZEKA);
}

int main() {
    srand(time(NULL));
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    snprintf(g_log_dir, sizeof(g_log_dir), "logi_%02d-%02d-%02d",
             tm->tm_hour, tm->tm_min, tm->tm_sec);
    mkdir(g_log_dir, 0755);

    g_semafor = utworz_nowy_semafor();
    ustaw_semafor(g_semafor, SEMAFOR_MAGAZYN, 1);
    ustaw_semafor(g_semafor, SEMAFOR_TASMA, 1);
    ustaw_semafor(g_semafor, SEMAFOR_PACZKI, 0);
    ustaw_semafor(g_semafor, SEMAFOR_CIEZAROWKI, 1);
    ustaw_semafor(g_semafor, SEMAFOR_ZAPIS, 1);
    ustaw_semafor(g_semafor, SEMAFOR_EXPRESS, 1);
    ustaw_semafor(g_semafor, SEMAFOR_GENERATOR, 1);
    ustaw_semafor(g_semafor, SEMAFOR_P4_CZEKA, 1);

    log_init(g_semafor, "magazyn.log", COL_GREEN);
    sem_log_init();
    char buf[256];

    int liczba_paczek = 0;
    int liczba_ciezarowek = 0;
    int nastepne_id = 0;
    
    Paczka* magazyn = generuj_paczke_poczatkowe(&liczba_paczek, &nastepne_id);
    Ciezarowka* ciezarowki = generuj_ciezarowke(&liczba_ciezarowek);
    
    if (!magazyn || !ciezarowki) {
        log_write("BLAD: Nie udalo sie wygenerowac danych\n");
        free(magazyn);
        free(ciezarowki);
        return 1;
    }
    
    pid_t pracownicy_pids[LICZBA_PRACOWNIKOW];
    pid_t *ciezarowki_pids = malloc(liczba_ciezarowek * sizeof(pid_t));
    pid_t generator_pid = -1;
    
    if (!ciezarowki_pids) {
        perror("malloc pids");
        free(magazyn);
        free(ciezarowki);
        return 1;
    }

    key_t klucz_magazyn = ftok(".", 'M');
    key_t klucz_tasma = ftok(".", 'T');
    key_t klucz_okienko = ftok(".", 'E');
    
    if (klucz_magazyn == -1 || klucz_tasma == -1 || klucz_okienko == -1) {
        perror("ftok");
        free(magazyn);
        free(ciezarowki);
        free(ciezarowki_pids);
        cleanup();
        return 1;
    }
    
    g_id_magazyn = shmget(klucz_magazyn, sizeof(Magazyn_wspolny), IPC_CREAT | 0600);
    if (g_id_magazyn == -1) {
        perror("shmget magazyn");
        free(magazyn);
        free(ciezarowki);
        free(ciezarowki_pids);
        cleanup();
        return 1;
    }
    
    g_wspolny = (Magazyn_wspolny *)shmat(g_id_magazyn, NULL, 0);
    if (g_wspolny == (Magazyn_wspolny *)(-1)) {
        perror("shmat magazyn");
        free(magazyn);
        free(ciezarowki);
        free(ciezarowki_pids);
        cleanup();
        return 1;
    }
    
    g_id_tasma = shmget(klucz_tasma, sizeof(Tasma), IPC_CREAT | 0600);
    if (g_id_tasma == -1) {
        perror("shmget tasma");
        free(magazyn);
        free(ciezarowki);
        free(ciezarowki_pids);
        cleanup();
        return 1;
    }
    
    g_tasma = (Tasma *)shmat(g_id_tasma, NULL, 0);
    if (g_tasma == (Tasma *)(-1)) {
        perror("shmat tasma");
        free(magazyn);
        free(ciezarowki);
        free(ciezarowki_pids);
        cleanup();
        return 1;
    }
    
    g_id_okienko = shmget(klucz_okienko, sizeof(OkienkoEkspresShm), IPC_CREAT | 0600);
    if (g_id_okienko == -1) {
        perror("shmget okienko");
        free(magazyn);
        free(ciezarowki);
        free(ciezarowki_pids);
        cleanup();
        return 1;
    }
    
    g_okienko = (OkienkoEkspresShm *)shmat(g_id_okienko, NULL, 0);
    if (g_okienko == (OkienkoEkspresShm *)(-1)) {
        perror("shmat okienko");
        free(magazyn);
        free(ciezarowki);
        free(ciezarowki_pids);
        cleanup();
        return 1;
    }
    
    g_okienko->ilosc = 0;
    g_okienko->ciezarowka_pid = 0;
    g_okienko->gotowe = 0;
    
    snprintf(buf, sizeof(buf), "Pamiec dzielona: magazyn=%d, tasma=%d, ekspresy=%d\n",
             g_id_magazyn, g_id_tasma, g_id_okienko);
    log_write(buf);

    generuj_tasme(g_tasma);
    ustaw_semafor(g_semafor, SEMAFOR_WOLNE_MIEJSCA, g_tasma->max_pojemnosc);
    
    g_wspolny->liczba_paczek = liczba_paczek;
    g_wspolny->nastepne_id = nastepne_id;
    g_wspolny->generowanie_aktywne = 0;
    
    for (int i = 0; i < liczba_paczek; i++) {
        g_wspolny->magazyn[i] = magazyn[i];
    }
    free(magazyn);
    magazyn = NULL;
    
    char arg_id[10], arg_shm[20], arg_sem[20], arg_shm_tasma[20], arg_shm_okienko[20];
    char arg_weight[20], arg_volume[20], arg_time[10];
    
    generator_pid = fork();
    if (generator_pid == 0) {
        sprintf(arg_shm, "%d", g_id_magazyn);
        sprintf(arg_sem, "%d", g_semafor);
        execl("./generator", "generator", arg_shm, arg_sem, g_log_dir, NULL);
        perror("execl generator");
        exit(1);
    } else if (generator_pid > 0) {
        snprintf(buf, sizeof(buf),"Uruchomiono generator paczek PID = %d\n", generator_pid);
        log_write(buf);
    } else {
        perror("fork generator");
    }
    
    for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            sprintf(arg_id, "%d", i + 1);
            sprintf(arg_shm, "%d", g_id_magazyn);
            sprintf(arg_sem, "%d", g_semafor);
            sprintf(arg_shm_tasma, "%d", g_id_tasma);
            sprintf(arg_shm_okienko, "%d", g_id_okienko);
            execl("./pracownicy", "pracownicy", arg_id, arg_shm, arg_sem, arg_shm_tasma, arg_shm_okienko, g_log_dir, NULL);
            exit(0);
        } else if (pid > 0) {
            pracownicy_pids[i] = pid;
            snprintf(buf, sizeof(buf),"Zatrudnilem pracownika %d o PID = %d\n", i + 1, pid);
            log_write(buf);
        } else {
            perror("fork pracownik");
        }
    }
    
    for (int i = 0; i < liczba_ciezarowek; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            sprintf(arg_id, "%d", ciezarowki[i].id_ciezarowki);
            sprintf(arg_shm_tasma, "%d", g_id_tasma);
            sprintf(arg_sem, "%d", g_semafor);
            sprintf(arg_weight, "%d", ciezarowki[i].waga_ciezarowki);
            sprintf(arg_volume, "%d", ciezarowki[i].pojemnosc_ciezarowki);
            sprintf(arg_time, "%ld", ciezarowki[i].czas_rozwozu);
            sprintf(arg_shm_okienko, "%d", g_id_okienko);
            execl("./ciezarowki", "ciezarowki", arg_id, arg_shm_tasma, arg_sem, 
                  arg_weight, arg_volume, arg_time, arg_shm_okienko, g_log_dir, NULL);
            exit(0);
        } else if (pid > 0) {
            ciezarowki_pids[i] = pid;
            snprintf(buf, sizeof(buf),"Uruchomiono ciezarowke %d, PID = %d\n", ciezarowki[i].id_ciezarowki, pid);
            log_write(buf);
        } else {
            perror("fork ciezarowka");
        }
    }
    
    pid_t dyspozytor_pid = uruchom_dyspozytora(g_id_tasma, g_semafor);
    
    for (int i = 0; i < LICZBA_PRACOWNIKOW; i++) {
        waitpid(pracownicy_pids[i], NULL, 0);
        snprintf(buf, sizeof(buf),"Pracownik %d zakonczyl swoja prace\n", i + 1);
        log_write(buf);
    }
    
    log_write("Wszyscy pracownicy zakończyli.\n");
    
    if (generator_pid > 0) {
        kill(generator_pid, SIGTERM);
        waitpid(generator_pid, NULL, 0);
        log_write("Generator paczek zakonczyl prace\n");
    }
    
    log_write("Wysylam SIGTERM do ciezarowek - dokoncza rozwoz...\n");
    for (int i = 0; i < liczba_ciezarowek; i++) {
        kill(ciezarowki_pids[i], SIGTERM);
    }

    odblokuj_ciezarowki(liczba_ciezarowek);
    
    for (int i = 0; i < liczba_ciezarowek; i++) {
        waitpid(ciezarowki_pids[i], NULL, 0);
        snprintf(buf, sizeof(buf),"Ciezarowka %d zakonczyla prace\n", i + 1);
        log_write(buf);
    }
    log_write("Wszystkie ciezarowki zakonczone.\n");
    
    free(ciezarowki);
    free(ciezarowki_pids);
    
    if (dyspozytor_pid > 0) {
        kill(dyspozytor_pid, SIGTERM);
        waitpid(dyspozytor_pid, NULL, 0);
        log_write("Dyspozytor zakonczyl prace\n");
    } else if (dyspozytor_pid == -1) {
        system("pkill -f './dyspozytor'");
        sleep(1);
        log_write("Dyspozytor (tmux) zakonczyl prace\n");
    }
    
    log_write("\n\n\n Symulacja zakonczyla sie poprawnie\n");
    cleanup();
    return 0;
}