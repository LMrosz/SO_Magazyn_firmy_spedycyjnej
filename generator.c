#include "utils.h"

int main(int argc, char *argv[]) {
    srand(time(NULL) ^ getpid());
    
    if (argc < 3) {
        fprintf(stderr, "Uzycie: %s shmid_magazyn semafor\n", argv[0]);
        return 1;
    }
    
    int shmid_magazyn = atoi(argv[1]);
    int semafor = atoi(argv[2]);
    
    ustaw_handlery_generator();
    otworz_plik_wyniki(semafor);
    
    Magazyn_wspolny *wspolny = (Magazyn_wspolny *)shmat(shmid_magazyn, NULL, 0);
    if (wspolny == (Magazyn_wspolny *)(-1)) {
        perror("shmat magazyn");
        return 1;
    }
    
    logi("GENERATOR: Uruchomiony (PID %d). Generuje %d paczek co %d s.\n", 
         getpid(), PACZEK_NA_TURE, INTERWAL_GENEROWANIA);

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
        
        logi("GENERATOR: Dodano %d paczek (ID %d-%d). W magazynie: %d. Lacznie wygenerowano: %d\n",
             wygenerowano, 
             wspolny->nastepne_id - wygenerowano, 
             wspolny->nastepne_id - 1,
             wspolny->liczba_paczek,
             wygenerowano_lacznie);
        
        semafor_v(semafor, SEMAFOR_MAGAZYN);
    }
    
    semafor_p(semafor, SEMAFOR_MAGAZYN);
    wspolny->generowanie_aktywne = 0;
    semafor_v(semafor, SEMAFOR_MAGAZYN);
    
    logi("GENERATOR: Zakonczyl prace. Lacznie wygenerowano: %d paczek.\n", wygenerowano_lacznie);
    
    shmdt(wspolny);
    zamknij_plik_wyniki();
    return 0;
}