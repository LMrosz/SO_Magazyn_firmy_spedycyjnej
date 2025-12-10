//Plik pracownicy.c
#include "utils.h"


int main(int argc, char *argv[]) {
	int id_pracownik = atoi(argv[1]);
	int shmid = atoi(argv[2]);
	semafor = atoi(argv[3]);
	Magazyn_wspolny *wspolny = (Magazyn_wspolny *)shmat(shmid, NULL, 0);
	if( wspolny == (Magazyn_wspolny *)(-1)){
                printf("Blad dostepu do pamieci dzielonej\n");
                return 1;
        }
	while(1){
                semafor_p(0);
                if(wspolny->liczba_paczek>0){
                        Paczka paczka = wspolny -> magazyn[wspolny -> liczba_paczek-1];
                        wspolny -> liczba_paczek = wspolny -> liczba_paczek - 1;
                        printf("Pracownik %d pobrał paczke: %d, zostało %d paczek\n",id_pracownik + 1, paczka.id,wspolny -> liczba_paczek);
                        semafor_v(0);
                        sleep(5);
			//pozniej dodac tu implementacje komunikacji z ciezarowka
                }
                else{
                        printf("Brak paczek w magazynie, pracownik %d czeka na nowe paczki\n",id_pracownik+1);
                        semafor_v(0);
                        sleep(30);
			break;
                }
        }
return 0;
}
