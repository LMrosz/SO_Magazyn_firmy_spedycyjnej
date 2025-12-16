/*
TO DO:
przygotowac funkcje zwracajace klucze, tworzace pamiec dzielona etc
dodac generowanie paczek w trakcie trwania symulacji
poprawic dostep do tasmy ciezarowce - tylko jedna przy tasmie
dodac sygnaly
P4 i paczki express
Poprawić indeksowanie pracowników
poprawic komunikat zamykania semafora
*/
//plik magazyn.c

#include "utils.h"

int g_id_magazyn = -1;
int g_id_tasma = -1;
int g_semafor = -1;
Magazyn_wspolny *g_wspolny = NULL;
Tasma *g_tasma = NULL;

void cleanup(void){
    if (g_wspolny && g_wspolny != (void*)-1) shmdt(g_wspolny);
    if (g_tasma && g_tasma != (void*)-1) shmdt(g_tasma);
    if (g_id_magazyn != -1) shmctl(g_id_magazyn, IPC_RMID, NULL);
    if (g_id_tasma != -1) shmctl(g_id_tasma, IPC_RMID, NULL);
    if (g_semafor != -1) usun_semafor(g_semafor);
}

void handle_sigint(int sig){
    (void)sig;
    const char msg[] = "\nSIGINT - sprzatam...\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    cleanup();
    kill(0, SIGTERM);
    _exit(0);
}

int main(){
	srand(time(NULL));

	struct sigaction sa; //ustawienie handlera od sygnalow
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

	int liczba_paczek = 0;
	int liczba_ciezarowek = 0;

	Paczka* magazyn = generuj_paczke(&liczba_paczek);//generowanie danych dla paczek
	Ciezarowka* ciezarowki = generuj_ciezarowke(&liczba_ciezarowek);//generowanie danych dla ciezarowek
	if (!magazyn || !ciezarowki) {
        fprintf(stderr, "Blad generowania danych ciezarowki lub magazynu\n");
        free(magazyn);
        free(ciezarowki);
        return 1;
    }

	pid_t pracownicy_pids[3];//pozniej dac na 4 bo P4 
	pid_t *ciezarowki_pids = malloc(liczba_ciezarowek * sizeof(pid_t));
    if (!ciezarowki_pids) {
        perror("malloc pids");
        free(magazyn);
        free(ciezarowki);
        return 1;
    }

	g_semafor = utworz_nowy_semafor();
	ustaw_semafor(g_semafor,0,1); //dostep do magazynu
	ustaw_semafor(g_semafor,1,1); //dostep do tasmy
	ustaw_semafor(g_semafor,3,0); //liczba paczek na tasmie

	key_t klucz_magazyn = ftok(".",'M');//TODO zmienic wartosc
	key_t klucz_tasma = ftok(".",'T');
	if (klucz_magazyn == -1 || klucz_tasma == -1) {
        perror("ftok");
        free(magazyn);
        free(ciezarowki);
        free(ciezarowki_pids);
        cleanup();
        return 1;
    }

	g_id_magazyn = shmget(klucz_magazyn, sizeof(Magazyn_wspolny), IPC_CREAT | 0600);
	if (g_id_magazyn == -1) {
        perror("shmget magazyn");
        free(magazyn);
        free(ciezarowki);
        free(ciezarowki_pids);
        cleanup();
        return 1;
    }
	
	g_wspolny = (Magazyn_wspolny *)shmat(g_id_magazyn, NULL, 0);
	if (g_wspolny == (Magazyn_wspolny *)(-1)) {
        perror("shmat magazyn");
        free(magazyn);
        free(ciezarowki);
        free(ciezarowki_pids);
        cleanup();
        return 1;
    }

	g_id_tasma = shmget(klucz_tasma, sizeof(Tasma), IPC_CREAT | 0600);
    if (g_id_tasma == -1) {
        perror("shmget tasma");
        free(magazyn);
        free(ciezarowki);
        free(ciezarowki_pids);
        cleanup();
        return 1;
    }
	
	g_tasma = (Tasma *)shmat(g_id_tasma, NULL, 0);
    if (g_tasma == (Tasma *)(-1)) {
        perror("shmat tasma");
        free(magazyn);
        free(ciezarowki);
        free(ciezarowki_pids);
        cleanup();
        return 1;
    }

	generuj_tasme(g_tasma);
	ustaw_semafor(g_semafor,2,g_tasma -> max_pojemnosc);
	g_wspolny -> liczba_paczek = liczba_paczek;
	for(int i = 0; i<liczba_paczek;i++){
		g_wspolny -> magazyn[i] = magazyn[i];
	}
	free(magazyn);
	magazyn = NULL;

	char arg_id[10];
	char arg_shm[20];
	char arg_sem[20];
	char arg_shm_tasma[20];
    char arg_weight[20];
	char arg_volume[20];
	char arg_time[10];
	
	for (int i = 0; i < 3; i++) {
 		pid_t pid = fork();
		if (pid == 0) {
			sprintf(arg_id, "%d", i+1);
        	sprintf(arg_shm, "%d", g_id_magazyn);
       		sprintf(arg_sem, "%d", g_semafor);
			sprintf(arg_shm_tasma, "%d", g_id_tasma);
        	execl("./pracownicy","pracownicy", arg_id, arg_shm, arg_sem,arg_shm_tasma,NULL);
			exit(0);
    	} else if (pid >0) {
			pracownicy_pids[i] = pid;
			printf("Zatrudniłem pracownika %d o PID = %d\n", i+1, pid);
		} else {
			perror("fork pracownik");
		}
	}

	for (int i = 0; i < liczba_ciezarowek; i++) {
                pid_t pid = fork();
                if (pid == 0) {
                	sprintf(arg_id, "%d", ciezarowki[i].id_ciezarowki);
                	sprintf(arg_shm_tasma, "%d", g_id_tasma);
					sprintf(arg_sem, "%d", g_semafor);
                	sprintf(arg_weight, "%d", ciezarowki[i].waga_ciezarowki);
                	sprintf(arg_volume, "%d", ciezarowki[i].pojemnosc_ciezarowki);
					sprintf(arg_time, "%ld",  ciezarowki[i].czas_rozwozu);
                	execl("./ciezarowki","ciezarowki", arg_id, arg_shm_tasma, arg_sem,arg_weight,arg_volume,arg_time,NULL);
                	exit(0);
                } else if (pid > 0 ){
					printf("Stworzono ciezarowke %d, PID = %d\n", ciezarowki[i].id_ciezarowki, pid);
				} else {
                    perror("fork ciezarowka");
                }
        }

	for (int i = 0; i < 3; i++) {
                waitpid(pracownicy_pids[i], NULL, 0);
                printf("Pracownik %d zakonczyl swoja prace\n", i+1);
        }

	//tymczasowe usuwanie ciezarowek
	printf("Rozpoczynam procedure zamykania systemu (wysylam trutki)...\n");
	for (int i = 0; i < liczba_ciezarowek; i++) {
        semafor_p(g_semafor, 2);
        semafor_p(g_semafor, 1);
        
        g_tasma->bufor[g_tasma->head].id = -1;
        g_tasma->bufor[g_tasma->head].waga = -1;
        g_tasma->head = (g_tasma->head + 1) % g_tasma->max_pojemnosc;
        g_tasma->aktualna_ilosc++;

        semafor_v(g_semafor, 1);
        semafor_v(g_semafor, 3);
    }

	for (int i = 0; i < liczba_ciezarowek; i++) {
        waitpid(ciezarowki_pids[i], NULL, 0);
        printf("Ciezarowka %d zakonczyla prace.\n", i + 1);
    }
    cleanup();
    free(ciezarowki);
    free(ciezarowki_pids);

    printf("\n\n\n Symulacja zakonczyla sie poprawnie\n");
return 0;
}
