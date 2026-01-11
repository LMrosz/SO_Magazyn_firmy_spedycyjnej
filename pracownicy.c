#include "utils.h"

int main(int argc, char *argv[]) {
    srand(time(NULL) ^ getpid());
    
    if (argc < 7) {
        fprintf(stderr, "Uzycie: %s id shmid_magazyn semafor shmid_tasma shmid_okienko log_dir\n", argv[0]);
        return 1;
    }
    
    int id_pracownik = atoi(argv[1]);
    int shmid_magazyn = atoi(argv[2]);
    int semafor = atoi(argv[3]);
    int shmid_tasma = atoi(argv[4]);
    int shmid_okienko = atoi(argv[5]);
    strncpy(g_log_dir, argv[6], sizeof(g_log_dir) - 1);
    
    ustaw_handlery_pracownik(id_pracownik);
    char nazwa[32];
    snprintf(nazwa, sizeof(nazwa), "pracownicy.log");
    log_init(semafor, nazwa, COL_CYAN);
    sem_log_init();
    
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
    
    char buf[256];

    if (id_pracownik == 4) {
        snprintf(buf, sizeof(buf),"Pracownik P4 (PID %d) - dostawa paczek ekspres.\n", getpid());
        log_write(buf);

        int pojemnosc_tablicy = 100;
        Paczka *paczki_ekspres = malloc(pojemnosc_tablicy * sizeof(Paczka));
        if (!paczki_ekspres) {
            perror("malloc paczki_ekspres");
            shmdt(wspolny);
            shmdt(tasma);
            shmdt(okienko);
            return 1;
        }
        
        int ile_oczekujacych = 0;
        
        while (!g_zakoncz_prace) {
            // Jeśli nie mamy paczek, czekaj na sygnał
            if (ile_oczekujacych == 0) {
                if (!g_dostarcz_ekspres) {
                    pause();
                    if (g_zakoncz_prace) break;
                    if (!g_dostarcz_ekspres) continue;
                }
                
                g_dostarcz_ekspres = 0;
                log_write("Pracownik P4: Otrzymano polecenie dostarczenia paczek ekspresowych!\n");
                
                // Zbierz WSZYSTKIE ekspresowe z magazynu
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
                        log_write("Pracownik P4: Blad realokacji pamieci!\n");
                        semafor_v(semafor, SEMAFOR_MAGAZYN);
                        continue;
                    }
                    paczki_ekspres = nowa;
                }

                ile_oczekujacych = 0;
                for (int i = wspolny->liczba_paczek - 1; i >= 0; i--) {
                    if (wspolny->magazyn[i].priorytet == EXPRES) {
                        paczki_ekspres[ile_oczekujacych++] = wspolny->magazyn[i];
                        wspolny->magazyn[i] = wspolny->magazyn[--wspolny->liczba_paczek];
                    }
                }
                semafor_v(semafor, SEMAFOR_MAGAZYN);
                
                if (ile_oczekujacych == 0) {
                    log_write("Pracownik P4: Brak paczek ekspresowych w magazynie!\n");
                    continue;
                }
                
                snprintf(buf, sizeof(buf),"Pracownik P4: Zebrano %d paczek ekspresowych z magazynu.\n", ile_oczekujacych);
                log_write(buf);
            }
            
            // ========== MAMY PACZKI - CZEKAJ NA CIĘŻARÓWKĘ ==========
            snprintf(buf, sizeof(buf),"Pracownik P4: Mam %d paczek ekspres, czekam na ciezarowke...\n", ile_oczekujacych);
            log_write(buf);
            
            // Czekaj aż ciężarówka zasygnalizuje gotowość (V na SEMAFOR_P4_CZEKA)
            semafor_p(semafor, SEMAFOR_P4_CZEKA);
            
            if (g_zakoncz_prace) break;
            
            // Sprawdź czy jest ciężarówka
            semafor_p(semafor, SEMAFOR_TASMA);
            pid_t ciezarowka_pid = tasma->ciezarowka;
            semafor_v(semafor, SEMAFOR_TASMA);
            
            if (ciezarowka_pid == 0) {
                // Fałszywy alarm - kontynuuj czekanie
                continue;
            }
            
            // Sprawdź czy przyszedł nowy sygnał - dobierz paczki
            if (g_dostarcz_ekspres) {
                g_dostarcz_ekspres = 0;
                log_write("Pracownik P4: Nowy sygnal - dobieram paczki z magazynu!\n");
                
                semafor_p(semafor, SEMAFOR_MAGAZYN);
                for (int i = wspolny->liczba_paczek - 1; i >= 0; i--) {
                    if (wspolny->magazyn[i].priorytet == EXPRES) {
                        if (ile_oczekujacych >= pojemnosc_tablicy) {
                            pojemnosc_tablicy += 50;
                            paczki_ekspres = realloc(paczki_ekspres, pojemnosc_tablicy * sizeof(Paczka));
                        }
                        paczki_ekspres[ile_oczekujacych++] = wspolny->magazyn[i];
                        wspolny->magazyn[i] = wspolny->magazyn[--wspolny->liczba_paczek];
                    }
                }
                semafor_v(semafor, SEMAFOR_MAGAZYN);
                
                snprintf(buf, sizeof(buf),"Pracownik P4: Teraz mam %d paczek ekspres.\n", ile_oczekujacych);
                log_write(buf);
            }
            
            snprintf(buf, sizeof(buf),"Pracownik P4: Znalazlem ciezarowke PID %d, dostarczam %d paczek...\n", 
                 ciezarowka_pid, ile_oczekujacych);
            log_write(buf);
            
            // Przygotuj okienko
            semafor_p(semafor, SEMAFOR_EXPRESS);
            
            okienko->ciezarowka_pid = ciezarowka_pid;
            okienko->ilosc = 0;
            okienko->gotowe = 0;
            
            for (int i = 0; i < ile_oczekujacych; i++) {
                okienko->paczki[okienko->ilosc++] = paczki_ekspres[i];
            }
            
            okienko->gotowe = 1;
            
            snprintf(buf, sizeof(buf),"Pracownik P4: Przygotowal %d paczek ekspres dla ciezarowki PID %d.\n", 
                 ile_oczekujacych, ciezarowka_pid);
            log_write(buf);
            
            semafor_v(semafor, SEMAFOR_EXPRESS);
            
            // Poczekaj aż ciężarówka odbierze
            int paczki_przed = ile_oczekujacych;
            int timeout = 0;
            
            while (!g_zakoncz_prace && timeout < 600) { // max 60 sekund
                semafor_p(semafor, SEMAFOR_EXPRESS);
                int pozostalo = okienko->ilosc;
                int gotowe = okienko->gotowe;
                semafor_v(semafor, SEMAFOR_EXPRESS);
                
                // Sprawdź czy ciężarówka nadal jest
                semafor_p(semafor, SEMAFOR_TASMA);
                pid_t aktualna = tasma->ciezarowka;
                semafor_v(semafor, SEMAFOR_TASMA);
                
                if (pozostalo == 0 && gotowe == 0) {
                    // Wszystko odebrane!
                    snprintf(buf, sizeof(buf),"Pracownik P4: Ciezarowka odebrala wszystkie %d paczek!\n", paczki_przed);
                    log_write(buf);
                    ile_oczekujacych = 0;
                    break;
                }
                
                if (aktualna != ciezarowka_pid && pozostalo > 0) {
                    // Ciężarówka odjechała - zostały paczki
                    snprintf(buf, sizeof(buf),"Pracownik P4: Ciezarowka odjechala, zostalo %d paczek - szukam nastepnej.\n", pozostalo);
                    log_write(buf);
                    
                    // Przenieś pozostałe z okienka
                    semafor_p(semafor, SEMAFOR_EXPRESS);
                    ile_oczekujacych = 0;
                    for (int i = 0; i < okienko->ilosc; i++) {
                        paczki_ekspres[ile_oczekujacych++] = okienko->paczki[i];
                    }
                    okienko->ilosc = 0;
                    okienko->gotowe = 0;
                    okienko->ciezarowka_pid = 0;
                    semafor_v(semafor, SEMAFOR_EXPRESS);
                    break;  // Wróć do początku pętli - czekaj na następną ciężarówkę
                }
                
                usleep(100000); // 100ms
                timeout++;
            }
            
            if (timeout >= 600 && ile_oczekujacych > 0) {
                log_write("Pracownik P4: Timeout - szukam nastepnej ciezarowki.\n");
                
                semafor_p(semafor, SEMAFOR_EXPRESS);
                ile_oczekujacych = okienko->ilosc;
                for (int i = 0; i < okienko->ilosc; i++) {
                    paczki_ekspres[i] = okienko->paczki[i];
                }
                okienko->ilosc = 0;
                okienko->gotowe = 0;
                okienko->ciezarowka_pid = 0;
                semafor_v(semafor, SEMAFOR_EXPRESS);
            }
        }
        
        // Zwróć nieodebrane paczki do magazynu
        if (ile_oczekujacych > 0) {
            snprintf(buf, sizeof(buf),"Pracownik P4: Zwracam %d nieodebranych paczek do magazynu.\n", ile_oczekujacych);
            log_write(buf);
            
            semafor_p(semafor, SEMAFOR_MAGAZYN);
            for (int i = 0; i < ile_oczekujacych; i++) {
                if (wspolny->liczba_paczek < MAX_PACZEK) {
                    wspolny->magazyn[wspolny->liczba_paczek++] = paczki_ekspres[i];
                }
            }
            semafor_v(semafor, SEMAFOR_MAGAZYN);
        }
        
        free(paczki_ekspres);
        log_write("Pracownik P4: Koncze prace.\n");
        
    } else {
        // ==================== PRACOWNICY 1-3 ====================
        snprintf(buf, sizeof(buf), "Pracownik %d start (PID %d)\n", id_pracownik, getpid());
        log_write(buf);

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
                Paczka paczka = wspolny->magazyn[znaleziono_indeks];
                wspolny->magazyn[znaleziono_indeks] = wspolny->magazyn[--wspolny->liczba_paczek];
                
                snprintf(buf, sizeof(buf),"Pracownik %d pobral paczke %d (Waga: %.2f). Pozostalo: %d\n",
                     id_pracownik, paczka.id, paczka.waga, wspolny->liczba_paczek);
                log_write(buf);

                semafor_v(semafor, SEMAFOR_MAGAZYN);
                semafor_p(semafor, SEMAFOR_WOLNE_MIEJSCA);
                
                if (g_zakoncz_prace) {
                    semafor_v(semafor, SEMAFOR_WOLNE_MIEJSCA);
                    break;
                }
                
                semafor_p(semafor, SEMAFOR_TASMA);
                
                while ((tasma->aktualna_waga + paczka.waga) > tasma->max_waga && !g_zakoncz_prace) {
                    snprintf(buf, sizeof(buf),"Pracownik %d: Tasma przeciazona (%.2f/%d kg), czekam...\n",
                         id_pracownik, tasma->aktualna_waga, tasma->max_waga);
                    
                    log_write(buf);
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
                
                snprintf(buf, sizeof(buf),"Pracownik %d polozyl paczke %d na tasmie. (Tasma: %d szt, %.2f kg)\n",
                     id_pracownik, paczka.id, tasma->aktualna_ilosc, tasma->aktualna_waga);
                log_write(buf);
                
                semafor_v(semafor, SEMAFOR_TASMA);
                semafor_v(semafor, SEMAFOR_PACZKI);
                
            } else {
                semafor_v(semafor, SEMAFOR_MAGAZYN);
                
                semafor_p(semafor, SEMAFOR_MAGAZYN);
                int generator_aktywny = wspolny->generowanie_aktywne;
                int pozostalo = wspolny->liczba_paczek;
                semafor_v(semafor, SEMAFOR_MAGAZYN);
                
                if (!generator_aktywny && pozostalo == 0) {
                    snprintf(buf, sizeof(buf),"Pracownik %d: Magazyn pusty i generator zatrzymany - koncze.\n", id_pracownik);
                    log_write(buf);
                    break;
                }
                
                sleep(2);
            }
        }
        
        if (g_zakoncz_prace) {
            snprintf(buf, sizeof(buf),"Pracownik %d: Koncze prace na polecenie.\n", id_pracownik);
            log_write(buf);
        }
    }
    
    shmdt(wspolny);
    shmdt(tasma);
    shmdt(okienko);
    sem_log_close();
    log_close();
    return 0;
}