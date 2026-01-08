#include "utils.h"

int main(int argc, char *argv[]) {
    srand(time(NULL) ^ getpid());
    
    if (argc < 6) {
        fprintf(stderr, "Uzycie: %s id shmid_magazyn semafor shmid_tasma shmid_okienko\n", argv[0]);
        return 1;
    }
    
    int id_pracownik = atoi(argv[1]);
    int shmid_magazyn = atoi(argv[2]);
    int semafor = atoi(argv[3]);
    int shmid_tasma = atoi(argv[4]);
    int shmid_okienko = atoi(argv[5]);
    
    ustaw_handlery_pracownik(id_pracownik);
    otworz_plik_wyniki(semafor);
    
    Magazyn_wspolny *wspolny = (Magazyn_wspolny *)shmat(shmid_magazyn, NULL, 0);
    if (wspolny == (Magazyn_wspolny *)(-1)) {
        perror("Blad dostepu do pamieci dzielonej magazynu");
        return 1;
    }
    
    Tasma *tasma = (Tasma *)shmat(shmid_tasma, NULL, 0);
    if (tasma == (Tasma *)(-1)) {
        perror("Blad dostepu do pamieci dzielonej tasmy");
        shmdt(wspolny);
        return 1;
    }
    
    OkienkoEkspresShm *okienko = (OkienkoEkspresShm *)shmat(shmid_okienko, NULL, 0);
    if (okienko == (OkienkoEkspresShm *)(-1)) {
        perror("shmat okienko");
        shmdt(wspolny);
        shmdt(tasma);
        return 1;
    }
    
    if (id_pracownik == 4) {
        logi("Pracownik P4 (PID %d) - dostawa paczek ekspres (BEZ LIMITU).\n", getpid());
        int pojemnosc_tablicy = 100;
        Paczka *paczki_ekspres = malloc(pojemnosc_tablicy * sizeof(Paczka));
        if (!paczki_ekspres) {
            perror("malloc paczki_ekspres");
            shmdt(wspolny);
            shmdt(tasma);
            shmdt(okienko);
            return 1;
        }
        
        while (!g_zakoncz_prace) {
            if (!g_dostarcz_ekspres) {
                pause();
                if (g_zakoncz_prace) break;
                continue;
            }
            
            g_dostarcz_ekspres = 0;
            logi("Pracownik P4: Otrzymano polecenie dostarczenia WSZYSTKICH paczek ekspresowych!\n");
            semafor_p(semafor, SEMAFOR_TASMA);
            pid_t ciezarowka_pid = tasma->ciezarowka;
            semafor_v(semafor, SEMAFOR_TASMA);
        
            if (ciezarowka_pid == 0) {
                logi("Pracownik P4: Brak ciezarowki przy tasmie!\n");
                continue;
            }
            
            int ile = 0;
            
            semafor_p(semafor, SEMAFOR_MAGAZYN);
            int liczba_ekspres = 0;
            for (int i = 0; i < wspolny->liczba_paczek; i++) {
                if (wspolny->magazyn[i].priorytet == EXPRES) {
                    liczba_ekspres++;
                }
            }
            if (liczba_ekspres > pojemnosc_tablicy) {
                pojemnosc_tablicy = liczba_ekspres + 50;
                Paczka *nowa = realloc(paczki_ekspres, pojemnosc_tablicy * sizeof(Paczka));
                if (!nowa) {
                    logi("Pracownik P4: Blad realokacji pamieci!\n");
                    semafor_v(semafor, SEMAFOR_MAGAZYN);
                    continue;
                }
                paczki_ekspres = nowa;
            }

            for (int i = wspolny->liczba_paczek - 1; i >= 0; i--) {
                if (wspolny->magazyn[i].priorytet == EXPRES) {
                    paczki_ekspres[ile++] = wspolny->magazyn[i];
                    wspolny->magazyn[i] = wspolny->magazyn[--wspolny->liczba_paczek];
                }
            }
            semafor_v(semafor, SEMAFOR_MAGAZYN);
            
            if (ile == 0) {
                logi("Pracownik P4: Brak paczek ekspresowych w magazynie!\n");
                continue;
            }
            
            logi("Pracownik P4: Znalazlem %d paczek ekspresowych, dostarczam WSZYSTKIE...\n", ile);
            semafor_p(semafor, SEMAFOR_EXPRESS);
            semafor_p(semafor, SEMAFOR_TASMA);
            
            if (tasma->ciezarowka != ciezarowka_pid) {
                semafor_v(semafor, SEMAFOR_TASMA);
                semafor_v(semafor, SEMAFOR_EXPRESS);
                semafor_p(semafor, SEMAFOR_MAGAZYN);
                for (int i = 0; i < ile; i++) {
                    if (wspolny->liczba_paczek < MAX_PACZEK) {
                        wspolny->magazyn[wspolny->liczba_paczek++] = paczki_ekspres[i];
                    }
                }
                semafor_v(semafor, SEMAFOR_MAGAZYN);
                
                logi("Pracownik P4: Ciezarowka odjechala, zwracam %d paczek do magazynu.\n", ile);
                continue;
            }
            semafor_v(semafor, SEMAFOR_TASMA);
            okienko->ciezarowka_pid = ciezarowka_pid;
            okienko->ilosc = 0;
            okienko->gotowe = 0;
            
            for (int i = 0; i < ile; i++) {
                okienko->paczki[okienko->ilosc++] = paczki_ekspres[i];
                logi("Pracownik P4: Wlozyl EKSPRES ID %d (%.2f kg) do okienka [%d/%d]\n",
                     paczki_ekspres[i].id, paczki_ekspres[i].waga, i + 1, ile);
            }
            
            okienko->gotowe = 1;
            
            semafor_v(semafor, SEMAFOR_EXPRESS);
            
            logi("Pracownik P4: Dostarczono WSZYSTKIE %d paczek ekspresowych przez okienko.\n", ile);
        }
        
        free(paczki_ekspres);
        logi("Pracownik P4: Koncze prace.\n");
        
    } else {
        int proby_pustego_magazynu = 0;
        const int MAX_PROB = 5;
        
        while (!g_zakoncz_prace) {
            semafor_p(semafor, SEMAFOR_MAGAZYN);
            
            int znaleziono_indeks = -1;
            for (int i = wspolny->liczba_paczek - 1; i >= 0; i--) {
                if (wspolny->magazyn[i].priorytet == ZWYKLA) {
                    znaleziono_indeks = i;
                    break;
                }
            }
            
            if (znaleziono_indeks >= 0) {
                proby_pustego_magazynu = 0;
                
                Paczka paczka = wspolny->magazyn[znaleziono_indeks];
                wspolny->magazyn[znaleziono_indeks] = wspolny->magazyn[--wspolny->liczba_paczek];
                
                logi("Pracownik %d pobral paczke %d (Waga: %.2f). Pozostalo: %d\n",
                     id_pracownik, paczka.id, paczka.waga, wspolny->liczba_paczek);
                
                semafor_v(semafor, SEMAFOR_MAGAZYN);
                
                semafor_p(semafor, SEMAFOR_WOLNE_MIEJSCA);
                semafor_p(semafor, SEMAFOR_TASMA);
                
                while ((tasma->aktualna_waga + paczka.waga) > tasma->max_waga && !g_zakoncz_prace) {
                    logi("Pracownik %d: Tasma przeciazona (%.2f/%d kg), czekam...\n",
                         id_pracownik, tasma->aktualna_waga, tasma->max_waga);
                    semafor_v(semafor, SEMAFOR_TASMA);
                    semafor_v(semafor, SEMAFOR_WOLNE_MIEJSCA);
                    sleep(1);
                    semafor_p(semafor, SEMAFOR_WOLNE_MIEJSCA);
                    semafor_p(semafor, SEMAFOR_TASMA);
                }
                
                if (g_zakoncz_prace) {
                    semafor_v(semafor, SEMAFOR_TASMA);
                    semafor_v(semafor, SEMAFOR_WOLNE_MIEJSCA);
                    break;
                }
                
                tasma->bufor[tasma->head] = paczka;
                tasma->head = (tasma->head + 1) % tasma->max_pojemnosc;
                tasma->aktualna_ilosc++;
                tasma->aktualna_waga += paczka.waga;
                
                logi("Pracownik %d polozyl paczke %d na tasmie. (Tasma: %d szt, %.2f kg)\n",
                     id_pracownik, paczka.id, tasma->aktualna_ilosc, tasma->aktualna_waga);
                
                semafor_v(semafor, SEMAFOR_TASMA);
                semafor_v(semafor, SEMAFOR_PACZKI);
                
            } else {
                semafor_v(semafor, SEMAFOR_MAGAZYN);
                proby_pustego_magazynu++;

                semafor_p(semafor, SEMAFOR_MAGAZYN);
                int generator_aktywny = wspolny->generowanie_aktywne;
                int pozostalo = wspolny->liczba_paczek;
                semafor_v(semafor, SEMAFOR_MAGAZYN);
                
                if (!generator_aktywny && pozostalo == 0) {
                    logi("Pracownik %d: Magazyn pusty i generator zatrzymany - koncze.\n", id_pracownik);
                    break;
                }
                
                if (proby_pustego_magazynu >= MAX_PROB && !generator_aktywny) {
                    logi("Pracownik %d: Koncze prace - brak paczek.\n", id_pracownik);
                    break;
                }
                
                logi("Pracownik %d: Brak zwyklych paczek (proba %d/%d), czekam...\n",
                     id_pracownik, proby_pustego_magazynu, MAX_PROB);
                sleep(2);
            }
        }
        
        if (g_zakoncz_prace) {
            logi("Pracownik %d: Koncze prace na polecenie.\n", id_pracownik);
        }
    }
    
    shmdt(wspolny);
    shmdt(tasma);
    shmdt(okienko);
    zamknij_plik_wyniki();
    return 0;
}