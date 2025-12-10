#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <sys/sem.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <sys/shm.h>

#define VOL_A 0.019456
#define VOL_B 0.046208
#define VOL_C 0.099712

#define MAX_PACZEK 18000

double simulation_time_speed=1.0;
time_t real_time = 60;
time_t simulation_time = 60; //real_time / simulation_time_speed;

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

typedef struct{
	int liczba_paczek;
	Paczka magazyn[MAX_PACZEK];
}Magazyn_wspolny;

int losuj (int min, int max){
        if (min > max) return min;
        return rand() % (max - min + 1) + min;
}

double losuj_d(double min, double max) {
    return min + (double)rand() / RAND_MAX * (max - min);
}

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

Paczka* generuj_paczke(int *liczba_paczek_out){
        int min = 3;
	int max = 100;
        //int max = 10000;
        double waga_paczek = 0;

        int liczba_paczek_zwyklych = 0;
        int liczba_paczek = losuj(min, max);

        Paczka *magazyn = (Paczka*)malloc(liczba_paczek * sizeof(Paczka));

        if (liczba_paczek_out != NULL){
        *liczba_paczek_out = liczba_paczek;
        }

        if (magazyn == NULL) {
                printf("Błąd alokacji pamieci tablicy dynamicznej \"magazyn\"!\n");
                return NULL;
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
        double objetosc_paczek = count_A * VOL_A + count_B * VOL_B + count_C * VOL_C;
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
        printf("\n-------------GENEROWANIE PACZEK-------------\n");
        printf("Wylosowano %d paczek\n",liczba_paczek);
        printf("Liczba paczek A wynosi: %d\n",count_A);
        printf("Liczba paczek B wynosi: %d\n",count_B);
        printf("Liczba paczek C wynosi: %d\n\n",count_C);
        printf("Liczba paczek ekspresowych wynosi: %d\n", przydzielone_ekspresy);
        printf("Liczba paczek zwyklych wynosi: %d\n",liczba_paczek_zwyklych);
        printf("Waga paczek wynosi: %.3f kg\n", waga_paczek);
        printf("Objetosc wszytskich paczek wynosi: %f m3\n", objetosc_paczek);

        return magazyn;
}

int semafor = 0;
static void utworz_nowy_semafor(void)
  {
    semafor=semget(1234,1,0600|IPC_CREAT);
    if (semafor==-1)
      {
        printf("Nie moglem utworzyc nowego semafora.\n");
        exit(EXIT_FAILURE);
      }
    else
      {
        printf("Semafor zostal utworzony : %d\n",semafor);
      }
  }

static void usun_semafor(void)
  {
    int sem;
    sem=semctl(semafor,0,IPC_RMID);
    if (sem==-1)
      {
        printf("Nie mozna usunac semafora.\n");
        exit(EXIT_FAILURE);
      }
    else
      {
        printf("Semafor zostal usuniety : %d\n",sem);
      }
  }

static void ustaw_semafor(int i)
  {
    int ustaw_sem;
    ustaw_sem=semctl(semafor,i,SETVAL,1);
    if (ustaw_sem==-1)
      {
        perror("Nie mozna ustawic semafora.\n");
        exit(EXIT_FAILURE);
      }
    else
      {
        printf("Semafor zostal ustawiony.\n");
      }
  }

static void semafor_p(int nr){
        int zmien_sem;
        struct sembuf bufor_sem;
        bufor_sem.sem_num=nr;
        bufor_sem.sem_op=-1;
        bufor_sem.sem_flg=0;
        zmien_sem=semop(semafor,&bufor_sem,1);
        if (zmien_sem==-1){
                if(errno == EINTR){
                        semafor_p(nr);
                }
                else{
                        printf("Nie moglem zamknac semafora.\n");
                        exit(EXIT_FAILURE);
                }
        }
        else{
                printf("Semafor zostal zamkniety.\n");
        }
}

static void semafor_v(int nr){
        int zmien_sem;
        struct sembuf bufor_sem;
        bufor_sem.sem_num=nr;
        bufor_sem.sem_op=1;
        bufor_sem.sem_flg=SEM_UNDO;
        zmien_sem=semop(semafor,&bufor_sem,1);
        if (zmien_sem==-1){
                printf("Nie moglem otworzyc semafora.\n");
                exit(EXIT_FAILURE);
        }
        else{
                printf("Semafor zostal otwarty.\n");
        }
}
#endif
