//Plik pracownicy.c
#include "utils.h"


int main(int argc, char *argv[]) {
	srand(time(NULL) ^ getpid()); //uzyskanie unikalnego ziarna XOR z pidem
	
	if (argc < 5) {
        fprintf(stderr, "Uzycie: %s id shmid_magazyn semafor shmid_tasma\n", argv[0]);
        return 1;
    }
	int id_pracownik = atoi(argv[1]);
	int shmid_magazyn = atoi(argv[2]);
	int semafor = atoi(argv[3]);
	int shmid_tasma = atoi(argv[4]);
	
	otworz_plik_wyniki(semafor);
	Magazyn_wspolny *wspolny = (Magazyn_wspolny *)shmat(shmid_magazyn, NULL, 0);
	if( wspolny == (Magazyn_wspolny *)(-1)){
                perror("Blad dostepu do pamieci dzielonej magazynu\n");
                return 1;
    }

	Tasma *tasma = (Tasma *)shmat(shmid_tasma, NULL, 0);
	if( tasma == (Tasma *)(-1)){
                perror("Blad dostepu do pamieci dzielonej tasmy\n");
                return 1;
    }

	int proby_pustego_magazynu = 0;
    const int MAX_PROB = 3;

	while(1){
            semafor_p(semafor, SEMAFOR_MAGAZYN);
            if(wspolny->liczba_paczek>0){
                Paczka paczka = wspolny -> magazyn[wspolny -> liczba_paczek-1];
                wspolny -> liczba_paczek--;
                logi("Pracownik %d pobrał paczke %d (Waga: %.2f). Pozostalo w magazynie: %d\n",
                id_pracownik, paczka.id, paczka.waga, wspolny->liczba_paczek);
                semafor_v(semafor, SEMAFOR_MAGAZYN);
				semafor_p(semafor, SEMAFOR_WOLNE_MIEJSCA);
				semafor_p(semafor, SEMAFOR_TASMA);
				while ((tasma->aktualna_waga + paczka.waga) > tasma->max_waga) {
                	logi("Pracownik %d: Tasma przeciazona (%.2f/%.d kg), czekam...\n",id_pracownik, tasma->aktualna_waga, tasma->max_waga);
                	semafor_v(semafor, SEMAFOR_TASMA);
                	semafor_v(semafor, SEMAFOR_WOLNE_MIEJSCA);
                	sleep(1);
                	semafor_p(semafor, SEMAFOR_WOLNE_MIEJSCA);
                	semafor_p(semafor, SEMAFOR_TASMA);
            	}

				tasma -> bufor[tasma -> head] = paczka;
				tasma -> head = (tasma -> head + 1) % tasma -> max_pojemnosc;
				tasma -> aktualna_ilosc++;
				tasma -> aktualna_waga += paczka.waga;

				logi("Pracownik %d polozyl paczke %d na tasmie. (Tasma: %d szt, %.2f kg)\n",
                id_pracownik, paczka.id, tasma->aktualna_ilosc, tasma->aktualna_waga);
				semafor_v(semafor, SEMAFOR_TASMA);
				semafor_v(semafor, SEMAFOR_PACZKI);
				} else {
                	semafor_v(semafor, SEMAFOR_MAGAZYN);
            		proby_pustego_magazynu++;
            		logi("Pracownik %d: Magazyn pusty (proba %d/%d)\n", id_pracownik, proby_pustego_magazynu, MAX_PROB);
            		if (proby_pustego_magazynu >= MAX_PROB) {
                		logi("Pracownik %d: Koncze prace.\n", id_pracownik);
                		break;
            		}
            		sleep(5);
                }
        }
	shmdt(wspolny);
	shmdt(tasma);
	zamknij_plik_wyniki();
return 0;
}
