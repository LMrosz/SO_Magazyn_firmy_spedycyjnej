#include "utils.h"

int main(int argc, char *argv[]) {
    srand(time(NULL) ^ getpid());
    
    if (argc < 4) {
        fprintf(stderr, "Uzycie: %s shmid_magazyn semafor log_dir\n", argv[0]);
        return 1;
    }
    
    int shmid_magazyn = atoi(argv[1]);
    int semafor = atoi(argv[2]);
    
    strncpy(g_log_dir, argv[3], sizeof(g_log_dir) - 1);
    ustaw_handlery_generator();
    log_init(semafor, "paczki.log", COL_MAGENTA);
    sem_log_init();
    
    Magazyn_wspolny *wspolny = (Magazyn_wspolny *)shmat(shmid_magazyn, NULL, 0);
    if (wspolny == (Magazyn_wspolny *)(-1)) {
        perror("shmat magazyn");
        return 1;
    }

    char buf[256];
    snprintf(buf, sizeof(buf),"Uruchomiony generator paczek (PID %d). Generuje %d paczek co %d s.\n", 
         getpid(), PACZEK_NA_TURE, INTERWAL_GENEROWANIA);
    log_write(buf);

    semafor_p(semafor, SEMAFOR_MAGAZYN);
    wspolny->generowanie_aktywne = 1;
    semafor_v(semafor, SEMAFOR_MAGAZYN);
    
    int wygenerowano_lacznie = 0;
    
    while (!g_zakoncz_prace) {
        sleep(INTERWAL_GENEROWANIA);
        
        if (g_zakoncz_prace) break;
        
        semafor_p(semafor, SEMAFOR_MAGAZYN);
        
        int wygenerowano = 0;
        for (int i = 0; i < PACZEK_NA_TURE && wspolny->liczba_paczek < MAX_PACZEK; i++) {
            Paczka p = generuj_pojedyncza_paczke(wspolny->nastepne_id++);
            wspolny->magazyn[wspolny->liczba_paczek++] = p;
            wygenerowano++;
        }
        
        wygenerowano_lacznie += wygenerowano;
        
        snprintf(buf, sizeof(buf),"Dodano %d paczek (ID %d-%d). W magazynie: %d. Lacznie wygenerowano: %d\n",
             wygenerowano, 
             wspolny->nastepne_id - wygenerowano, 
             wspolny->nastepne_id - 1,
             wspolny->liczba_paczek,
             wygenerowano_lacznie);
        log_write(buf);
        semafor_v(semafor, SEMAFOR_MAGAZYN);
    }
    
    semafor_p(semafor, SEMAFOR_MAGAZYN);
    wspolny->generowanie_aktywne = 0;
    semafor_v(semafor, SEMAFOR_MAGAZYN);
    
    snprintf(buf, sizeof(buf),"Lacznie wygenerowano: %d paczek.\n", wygenerowano_lacznie);
    log_write(buf);
    
    shmdt(wspolny);
    sem_log_close();
    log_close();
    return 0;
}