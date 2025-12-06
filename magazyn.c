#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define VOL_A 0.019456
#define VOL_B 0.046208
#define VOL_C 0.099712

//double simulation_time_speed=1.0;

int losuj (int min, int max){
	if (min > max) return min;
	return rand() % (max - min + 1) + min;
}

double losuj_d(double min, double max) {
    return min + (double)rand() / RAND_MAX * (max - min);
}

typedef enum {
    A,
    B,
    C
} TypPaczki;

typedef enum {
    ZWYKLA,
    EXPRES
} Priorytet;

typedef struct {
	int id;
	double waga; //wartosc losowa z zakresu 0.1 do 25 kg
	TypPaczki typ;    //typ A B C
	double objetosc; //objetosc wyliczna za pomoca typu
	Priorytet priorytet; // 0 - paczka zwykla , 1- paczka ekspresowa
} Paczka;

const char* nazwa_typu(TypPaczki t) {
    switch (t) {
        case A: return "A (Mala)";
        case B: return "B (Srednia)";
        case C: return "C (Duza)";
        default: return "?";
    }
}

const char* nazwa_priorytetu(Priorytet p) {
    return (p == EXPRES) ? "EKSPRES" : "Zwykla";
}

int main() {
	srand(time(NULL));
	//generowanie danych dla paczek
	int min = 3;
	int max = 1000;
	double waga_paczek = 0;

	int liczba_paczek_zwyklych = 0;
	int liczba_paczek = losuj(min, max);

	Paczka *magazyn = (Paczka*)malloc(liczba_paczek * sizeof(Paczka));

	if (magazyn == NULL) {
    		printf("Błąd: Brak pamięci RAM!\n");
    		return 1;
	}

	int count_A, count_B, count_C;
	if (liczba_paczek >= 3) {
		count_A = losuj(1, liczba_paczek - 2);
		count_B = losuj(1, liczba_paczek - count_A - 1);
		count_C = liczba_paczek - (count_A + count_B);
	} else {
		count_A = liczba_paczek; count_B = 0; count_C = 0;
	}

	int max_ekspres = (int)(liczba_paczek * 0.25);
	int ile_ekspresow = (max_ekspres > 0) ? losuj(1, max_ekspres) : 0;
	int przydzielone_ekspresy = 0;
	for (int i = 0; i < liczba_paczek; i++) {
		magazyn[i].id = i + 1;
		if (i < count_A) {
			magazyn[i].typ = A;
			magazyn[i].objetosc = VOL_A;
			magazyn[i].waga = losuj_d(0.1, 5.0);
		}
		else if (i < count_A + count_B) {
			magazyn[i].typ = B;
			magazyn[i].objetosc = VOL_B;
			magazyn[i].waga = losuj_d(5.0, 15.0);
		}
		else {
			magazyn[i].typ = C;
			magazyn[i].objetosc = VOL_C;
			magazyn[i].waga = losuj_d(15.0, 25.0);
		}

		magazyn[i].waga = round(magazyn[i].waga * 1000) / 1000.0;
		waga_paczek+=magazyn[i].waga;

		if (przydzielone_ekspresy < ile_ekspresow && (rand() % 100 < 30)) {
			magazyn[i].priorytet = EXPRES;
			przydzielone_ekspresy++;
		} else if (przydzielone_ekspresy < ile_ekspresow && i >= liczba_paczek - (ile_ekspresow - przydzielone_ekspresy)) {
			magazyn[i].priorytet = EXPRES;
			przydzielone_ekspresy++;
		} else {
			magazyn[i].priorytet = ZWYKLA;
		}
	}
	printf("\n%-4s | %-12s | %-10s | %-10s | %-10s\n", "ID", "TYP", "WAGA (kg)", "OBJ (m3)", "PRIORYTET");
	printf("-----------------------------------------------------------\n");
	for (int i = 0; i < liczba_paczek; i++) {
		printf("%-4d | %-12s | %-10.3f | %-10.6f | %-10s\n",
		magazyn[i].id,
		nazwa_typu(magazyn[i].typ),
		magazyn[i].waga,
		magazyn[i].objetosc,
		nazwa_priorytetu(magazyn[i].priorytet));
	}
	printf("\nPodsumowanie:\n");
	printf("Razem paczek: %d (w tym %d ekspresowych)\n", liczba_paczek, przydzielone_ekspresy);
	liczba_paczek_zwyklych = liczba_paczek - przydzielone_ekspresy;
        double objetosc_paczek = count_A * VOL_A + count_B * VOL_B + count_C * VOL_C;
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

	time_t min_czas_ciezarowki = 8;
	time_t max_czas_ciezarowki = 50;
	time_t czas_ciezarowki = losuj(min_czas_ciezarowki, max_czas_ciezarowki);

	//generowanie danych dla tasmy
	int min_liczba_paczek_tasma = 3;
	int max_liczba_paczek_tasma = 30;
	int liczba_paczek_tasma = losuj(min_liczba_paczek_tasma, max_liczba_paczek_tasma);

	int min_waga_tasma = 25;
	int max_waga_tasma = 250;
	int waga_tasma = losuj(min_waga_tasma, max_waga_tasma);

	printf("\n-------------GENEROWANIE PACZEK-------------\n");
	printf("Wylosowano %d paczek\n",liczba_paczek);
	printf("Liczba paczek A wynosi: %d\n",count_A);
	printf("Liczba paczek B wynosi: %d\n",count_B);
	printf("Liczba paczek C wynosi: %d\n\n",count_C);
	printf("Liczba paczek ekspresowych wynosi: %d\n", przydzielone_ekspresy);
	printf("Liczba paczek zwyklych wynosi: %d\n",liczba_paczek_zwyklych);
	printf("Waga paczek wynosi: %.3f kg\n", waga_paczek);
	printf("Objetosc wszytskich paczek wynosi: %f m3\n", objetosc_paczek);

	printf("\n-------------GENEROWANIE CIEZAROWEK-------------\n");
	printf("Liczba ciezarowek wynosi: %d\n", liczba_ciezarowek);
	printf("Dopuszczalna waga ladunku wynosi: %d kg\n", waga_ciezarowka);
	printf("Dopuszczalna objetosc ladunku: %d m^3\n", pojemnosc_ciezarowki);
	printf("Czas rozwozu paczek dla ciezarowki wynosi: %ld s\n",czas_ciezarowki);

	printf("\n-------------GENEROWANIE TASMY-------------\n");
	printf("Maksymalna liczba paczek na tasmie: %d\n", liczba_paczek_tasma);
	printf("Maksymalny udzwig tasmy: %d kg\n",waga_tasma);

	free(magazyn);

return 0;
}

