#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

double simulation_time_speed=1.0;

int main() {
	srand(time(NULL));
	//generowanie danych dla paczek
	int min=1;
	int max=250;

	int liczba_paczek = rand() % (max - min + 1) + min;
	int liczba_paczek_A = (rand() % (liczba_paczek - 2)) + 1;
	int liczba_paczek_B = (rand() % ((liczba_paczek - liczba_paczek_A) - 1)) + 1;
	int liczba_paczek_C = liczba_paczek - (liczba_paczek_A + liczba_paczek_B);
	int liczba_paczek_ekspres = (rand() % (int)((liczba_paczek - 1)*0.25)) + 1;
	int liczba_paczek_zwyklych = liczba_paczek - liczba_paczek_ekspres;

	const double min_waga=0.1;
	const double max_waga=25;
	double waga_paczek = 0;
	for(int i=1; i<=liczba_paczek;i++){//nalezalo by dodac dopasowanie wagi do rozmiaru, 3 petle do losowania wagi?
		double waga = round( (min_waga + (double)rand() / RAND_MAX * (max_waga - min_waga)) * 1000 ) / 1000.0;
		printf("Waga paczki %d wynosi %.3f kg\n",i,waga);
		waga_paczek+=waga;
	}

	//generowanie danych dla ciezarowek
	int min_ciezarowek = 1;
	int max_ciezarowek = 50;
	int liczba_ciezarowek = rand() % (max_ciezarowek - min_ciezarowek + 1) + min_ciezarowek;

	int min_waga_c = 100;
	int max_waga_c = 2000;
	int waga_ciezarowka = rand() % (max_waga_c - min_waga_c + 1) + min_waga_c;

	//nalezy sie zastanowic czy pracowac na danych z m^3 czy cm^3 w poleceniu jest m
	//sprawdzic realnosc danych, w zwiazku z rozmiarami i wagami paczek
	int min_v_ciezarowki = 10;
	int max_v_ciezarowki = 100;
	int pojemnosc_ciezarowki = rand() % (max_v_ciezarowki - min_v_ciezarowki + 1) + min_v_ciezarowki;

	time_t min_czas_ciezarowki = 8;
	time_t max_czas_ciezarowki = 50;
	time_t czas_ciezarowki = rand() % (max_czas_ciezarowki - min_czas_ciezarowki + 1) + min_czas_ciezarowki;
	//generowanie danych dla tasmy
	int min_liczba_paczek_tasma = 3;
	int max_liczba_paczek_tasma = 30;
	int liczba_paczek_tasma = rand() % (max_liczba_paczek_tasma - min_liczba_paczek_tasma + 1) + min_liczba_paczek_tasma;

	int min_waga_tasma = 2;
	int max_waga_tasma = 250;
	int waga_tasma = rand() % (max_waga_tasma - min_waga_tasma + 1) + min_waga_tasma;

	printf("\n-------------GENEROWANIE PACZEK-------------\n");
	printf("Wylosowano %d paczek\n",liczba_paczek);
	printf("Liczba paczek A wynosi: %d\n",liczba_paczek_A);
	printf("Liczba paczek B wynosi: %d\n",liczba_paczek_B);
	printf("Liczba paczek C wynosi: %d\n\n",liczba_paczek_C);
	printf("Liczba paczek ekspresowych wynosi: %d\n", liczba_paczek_ekspres);
	printf("Liczba paczek zwyklych wynosi: %d\n",liczba_paczek_zwyklych);
	printf("Waga paczek wynosi: %.3f kg\n", waga_paczek);

	printf("\n-------------GENEROWANIE CIEZAROWEK-------------\n");
	printf("Liczba ciezarowek wynosi: %d\n", liczba_ciezarowek);
	printf("Dopuszczalna waga ladunku wynosi: %d kg\n", waga_ciezarowka);
	printf("Dopuszczalna objetosc ladunku: %d m^3\n", pojemnosc_ciezarowki);
	printf("Czas rozwozu paczek dla ciezarowki wynosi: %ld s\n",czas_ciezarowki);

	printf("\n-------------GENEROWANIE TASMY-------------\n");
	printf("Maksymalna liczba paczek na tasmie: %d\n", liczba_paczek_tasma);
	printf("Maksymalny udzwig tasmy: %d kg\n",waga_tasma);

return 0;
}
