//Plik ciezarowki.c
#include "utils.h"

int main(int argc, char *argv[]) {
    srand(time(NULL) ^ getpid());
	if (argc < 7) {
        fprintf(stderr, "Uzycie: %s id shmid semafor waga pojemnosc czas\n", argv[0]);
        return 1;
    }

	int id_ciezarowki = atoi(argv[1]);
	int shmid = atoi(argv[2]);
	int semafor = atoi(argv[3]);
    int dopuszczalna_waga = atoi(argv[4]);
	int dopuszczalna_pojemnosc = atoi(argv[5]);
	time_t czas_rozwozu = atoi(argv[6]);

	double waga = 0;
	double pojemnosc = 0;
	int liczba_paczek_c = 0;

    Tasma *tasma = (Tasma *)shmat(shmid, NULL, 0);
	if (tasma == (Tasma *)(-1)) {
        perror("shmat tasma");
        return 1;
    }
    while(1){
        semafor_p(semafor, 4); // zablokowanie dostepu do tasmy innym ciezarowkom
		semafor_p(semafor, 3);
		semafor_p(semafor, 1);
		if (tasma->aktualna_ilosc == 0) {
            semafor_v(semafor, 1);
            semafor_v(semafor, 3);
            semafor_v(semafor, 4);
            usleep(100000);
            continue;
        }

		Paczka aktualna_paczka = tasma->bufor[tasma->tail];
		if (aktualna_paczka.waga < 0) {
            tasma->tail = (tasma->tail + 1) % tasma->max_pojemnosc;
            tasma->aktualna_ilosc--;
            semafor_v(semafor, 2);
            semafor_v(semafor, 1);
            semafor_v(semafor, 4);
            printf("Ciezarowka %d: Otrzymano sygnal zakonczenia.\n", id_ciezarowki);
            break;
        }

		if(waga + aktualna_paczka.waga <= dopuszczalna_waga
		&& czy_pojemnosc(aktualna_paczka.typ, dopuszczalna_pojemnosc - pojemnosc)){
            waga += aktualna_paczka.waga;
            pojemnosc += aktualna_paczka.objetosc;
            tasma->aktualna_waga -=aktualna_paczka.waga;
            tasma->aktualna_ilosc--;
            tasma->tail = (tasma->tail + 1) % tasma->max_pojemnosc;
            liczba_paczek_c++;

            printf("Ciezarowka %d zaladowala paczke ID %d (Waga: %.2f). Stan ladunku: %.2f/%d kg\n",
            id_ciezarowki, aktualna_paczka.id, aktualna_paczka.waga, waga, dopuszczalna_waga);
			
			semafor_v(semafor, 2);
            semafor_v(semafor, 1);
            semafor_v(semafor, 4);
		} else if (liczba_paczek_c == 0) {
            // Pusta ciezarowka nie moze zabrac paczki - testy - nie powinno dojsc do takiej sytulacji
            printf("Ciezarowka %d: Paczka %d (%.2f kg) za duza - USUNIETA!\n",
                   id_ciezarowki, aktualna_paczka.id, aktualna_paczka.waga);
            
            tasma->aktualna_waga -= aktualna_paczka.waga;
            tasma->aktualna_ilosc--;
            tasma->tail = (tasma->tail + 1) % tasma->max_pojemnosc;
            
            semafor_v(semafor, 2);
            semafor_v(semafor, 1);
            semafor_v(semafor, 4);
            
        } else {
            // Ciężarówka pełna
            semafor_v(semafor, 1);
            semafor_v(semafor, 3);
            semafor_v(semafor, 4);
            
            printf("Ciezarowka %d PELNA (%.2f/%d kg, %d paczek). Odjezdza na %ld s.\n",
                   id_ciezarowki, waga, dopuszczalna_waga, liczba_paczek_c, czas_rozwozu);
            
            sleep(czas_rozwozu);
            
            printf("Ciezarowka %d wrocila pusta.\n", id_ciezarowki);
            waga = 0;
            pojemnosc = 0;
            liczba_paczek_c = 0;
		}
	}
	if (liczba_paczek_c > 0) {
        printf("Ciezarowka %d: Ostatni kurs z %d paczkami...\n", 
        id_ciezarowki, liczba_paczek_c);
        sleep(czas_rozwozu);
    }

    shmdt(tasma);
    printf("Ciezarowka %d zakonczyla prace.\n", id_ciezarowki);
    return 0;
}
