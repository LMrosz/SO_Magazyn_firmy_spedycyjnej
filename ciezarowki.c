//plik ciezarowki.c
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

    otworz_plik_wyniki(semafor);

    double waga = 0;
    double pojemnosc = 0;
    int liczba_paczek_c = 0;
    int zakonczenie = 0; //flaga zakonczenia programu - tymczasowe

    Tasma *tasma = (Tasma *)shmat(shmid, NULL, 0);
    if (tasma == (Tasma *)(-1)) {
        perror("shmat tasma");
        return 1;
    }

    while (!zakonczenie) {
        semafor_p(semafor, SEMAFOR_CIEZAROWKI);
        
        logi("Ciezarowka %d zajela stanowisko zaladunku.\n", id_ciezarowki);

        while (1) {
            semafor_p(semafor, SEMAFOR_PACZKI);
            semafor_p(semafor, SEMAFOR_TASMA);

            if (tasma->aktualna_ilosc == 0) {
                semafor_v(semafor, SEMAFOR_TASMA);
                semafor_v(semafor, SEMAFOR_PACZKI);
                continue;
            }
            Paczka aktualna_paczka = tasma->bufor[tasma->tail];

            if (aktualna_paczka.waga < 0) {
                tasma->tail = (tasma->tail + 1) % tasma->max_pojemnosc;
                tasma->aktualna_ilosc--;
                
                semafor_v(semafor, SEMAFOR_WOLNE_MIEJSCA);  
                semafor_v(semafor, SEMAFOR_TASMA);  
                
                logi("Ciezarowka %d: Otrzymano sygnal zakonczenia.\n", id_ciezarowki);
                zakonczenie = 1;
                break;
            }

            if (waga + aktualna_paczka.waga <= dopuszczalna_waga &&
                czy_pojemnosc(aktualna_paczka.typ, dopuszczalna_pojemnosc - pojemnosc)) {
                
                waga += aktualna_paczka.waga;
                pojemnosc += aktualna_paczka.objetosc;
                tasma->aktualna_waga -= aktualna_paczka.waga;
                tasma->aktualna_ilosc--;
                tasma->tail = (tasma->tail + 1) % tasma->max_pojemnosc;
                liczba_paczek_c++;

                logi("Ciezarowka %d zaladowala paczke ID %d (Waga: %.2f). Stan ladunku: %.2f/%d kg\n",
                     id_ciezarowki, aktualna_paczka.id, aktualna_paczka.waga, waga, dopuszczalna_waga);

                semafor_v(semafor, SEMAFOR_WOLNE_MIEJSCA);  
                semafor_v(semafor, SEMAFOR_TASMA); 
                
            } else {
                semafor_v(semafor, SEMAFOR_PACZKI);
                semafor_v(semafor, SEMAFOR_TASMA);
                break;  
            }
        }

        semafor_v(semafor, SEMAFOR_CIEZAROWKI);

        if (!zakonczenie && liczba_paczek_c > 0) {
            logi("Ciezarowka %d PELNA (%.2f/%d kg, %d paczek). Odjezdza na %ld s.\n",
                 id_ciezarowki, waga, dopuszczalna_waga, liczba_paczek_c, czas_rozwozu);
            
            sleep(czas_rozwozu);
            
            logi("Ciezarowka %d wrocila pusta.\n", id_ciezarowki);
            waga = 0;
            pojemnosc = 0;
            liczba_paczek_c = 0;
        }
    }

    if (liczba_paczek_c > 0) {
        logi("Ciezarowka %d: Ostatni kurs z %d paczkami...\n", 
             id_ciezarowki, liczba_paczek_c);
        sleep(czas_rozwozu);
    }

    shmdt(tasma);
    logi("Ciezarowka %d zakonczyla prace.\n", id_ciezarowki);
    zamknij_plik_wyniki();
    return 0;
}