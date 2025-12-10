//plik magazyn.c
#include <stdio.h>
#include "utils.h"

int main() {
	srand(time(NULL));
	int liczba_paczek = 0;
	//generowanie danych dla paczek
	Paczka* magazyn = generuj_paczke(&liczba_paczek);
	//generowanie danych dla ciezarowek
	int min_ciezarowek = 1;
	int max_ciezarowek = 50;
	int liczba_ciezarowek = losuj(min_ciezarowek, max_ciezarowek);

	int min_waga_c = 500;
	int max_waga_c = 20000;
	int waga_ciezarowka = losuj(min_waga_c, max_waga_c);

	int min_v_ciezarowki = 10;
	int max_v_ciezarowki = 100;
	int pojemnosc_ciezarowki = losuj(min_v_ciezarowki, max_v_ciezarowki);

	time_t min_czas_ciezarowki = 10;
	time_t max_czas_ciezarowki = 50;
	time_t czas_ciezarowki = losuj(min_czas_ciezarowki, max_czas_ciezarowki);

	//generowanie danych dla tasmy
	int min_liczba_paczek_tasma = 3;
	int max_liczba_paczek_tasma = 30;
	int liczba_paczek_tasma = losuj(min_liczba_paczek_tasma, max_liczba_paczek_tasma);

	int min_waga_tasma = 25;
	int max_waga_tasma = 250;
	int waga_tasma = losuj(min_waga_tasma, max_waga_tasma);

	printf("\n-------------GENEROWANIE CIEZAROWEK-------------\n");
	printf("Liczba ciezarowek wynosi: %d\n", liczba_ciezarowek);
	printf("Dopuszczalna waga ladunku wynosi: %d kg\n", waga_ciezarowka);
	printf("Dopuszczalna objetosc ladunku: %d m^3\n", pojemnosc_ciezarowki);
	printf("Czas rozwozu paczek dla ciezarowki wynosi: %ld s\n",czas_ciezarowki);

	printf("\n-------------GENEROWANIE TASMY-------------\n");
	printf("Maksymalna liczba paczek na tasmie: %d\n", liczba_paczek_tasma);
	printf("Maksymalny udzwig tasmy: %d kg\n",waga_tasma);
	utworz_nowy_semafor();
	ustaw_semafor(0);
	key_t klucz = ftok(".",'p');
	if( klucz == -1){
		printf("Blad utworzenia klucza\n");
		return 1;
	}
	int id = shmget(klucz, sizeof(Magazyn_wspolny), IPC_CREAT | 0600);
	Magazyn_wspolny *wspolny = (Magazyn_wspolny *)shmat(id, NULL, 0);
	if( wspolny == (Magazyn_wspolny *)(-1)){
                printf("Blad dostepu do pamieci dzielonej\n");
                return 1;
        }
	char arg_id[10];
	char arg_shm[20];
	char arg_sem[20];

	semafor_p(0);
	wspolny -> liczba_paczek = liczba_paczek;
	for(int i = 0; i<liczba_paczek;i++){
		wspolny -> magazyn[i] = magazyn[i];
	}
	semafor_v(0);

	for (int i = 0; i < 3; i++) {
 		pid_t pid = fork();
		if (pid == 0) {
		sprintf(arg_id, "%d", i);
        	sprintf(arg_shm, "%d", id);
       		sprintf(arg_sem, "%d", semafor);
        	execl("./pracownicy","pracownicy", arg_id, arg_shm, arg_sem,NULL);
		exit(0);
    		}
		else{
			printf("Zatrudniłem pracownika %d o PID = %d\n", i, pid);
		}
	}
	wait(NULL);
	wait(NULL);
	wait(NULL);
	printf("Pracownicy zakonczyli swoja prace\n");
	usun_semafor();
    	shmdt(wspolny);//obsluzyc blad
    	shmctl(id, IPC_RMID, NULL);//obsluzyc blad
	free(magazyn);
	magazyn = NULL;
return 0;
}
