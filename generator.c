#include "utils.h"

int main(int argc, char *argv[]) {
    srand(time(NULL) ^ getpid());
    
    if (argc < 7) {
        fprintf(stderr, "Uzycie: %s shmid_magazyn semafor log_dir interwal paczek_tura procent_ekspres\n", argv[0]);
        return 1;
    }
    
    int shmid_magazyn = atoi(argv[1]);
    int semafor = atoi(argv[2]);
    strncpy(g_log_dir, argv[3], sizeof(g_log_dir) - 1);
    int interwal_generowania = atoi(argv[4]);
    int paczek_na_ture = atoi(argv[5]);
    g_config.procent_ekspres = atoi(argv[6]);
    
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
         getpid(), paczek_na_ture, interwal_generowania);
    log_write(buf);

    semafor_p(semafor, SEMAFOR_MAGAZYN);
    wspolny->generowanie_aktywne = 1;
    semafor_v(semafor, SEMAFOR_MAGAZYN);
    
    int wygenerowano_lacznie = 0;
    
    while (!g_zakoncz_prace) {
        sleep(interwal_generowania);
        
        if (g_zakoncz_prace) break;
        
        semafor_p(semafor, SEMAFOR_MAGAZYN);
        
        int wygenerowano = 0;
        for (int i = 0; i < paczek_na_ture && wspolny->liczba_paczek < MAX_PACZEK; i++) {
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