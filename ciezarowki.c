#include "utils.h"

int main(int argc, char *argv[]) {
    srand(time(NULL) ^ getpid());
    
    if (argc < 10) {
        fprintf(stderr, "Uzycie: %s id shm_tasma sem waga pojemnosc czas shm_okienko log_dir kolejka\n", argv[0]);
        return 1;
    }
    
    int id = atoi(argv[1]);
    int shm_tasma = atoi(argv[2]);
    int sem = atoi(argv[3]);
    int max_waga = atoi(argv[4]);
    int max_pojemnosc = atoi(argv[5]);
    time_t czas_rozwozu = atoi(argv[6]);
    int shm_okienko = atoi(argv[7]);
    strncpy(g_log_dir, argv[8], sizeof(g_log_dir) - 1);
    int kolejka = atoi(argv[9]);
    
    ustaw_handlery_ciezarowka();
    log_init(sem, "ciezarowki.log", COL_GREEN);
    sem_log_init();
    char buf[256];
    
    Tasma *tasma = (Tasma *)shmat(shm_tasma, NULL, 0);
    if (tasma == (Tasma *)(-1)) {
        perror("shmat tasma");
        return 1;
    }
    
    OkienkoEkspresShm *okienko = (OkienkoEkspresShm *)shmat(shm_okienko, NULL, 0);
    if (okienko == (OkienkoEkspresShm *)(-1)) {
        perror("shmat okienko");
        shmdt(tasma);
        return 1;
    }

    double waga = 0;
    double pojemnosc = 0;
    int liczba_paczek = 0;
    int ekspres = 0;

    snprintf(buf, sizeof(buf), "Ciezarowka %d (PID %d) rozpoczyna prace [W=%dkg, V=%dm3, Ti=%lds]\n", 
        id, getpid(), max_waga, max_pojemnosc, czas_rozwozu);
    log_write(buf);

    while (!g_zakoncz_prace && !g_zakoncz_przyjmowanie) {
        if (!semafor_p(sem, SEMAFOR_CIEZAROWKI)) {
            break;
        }

        if (g_zakoncz_prace || g_zakoncz_przyjmowanie) {
            semafor_v(sem, SEMAFOR_CIEZAROWKI);
            break;
        }
        
        semafor_p(sem, SEMAFOR_TASMA);
        tasma->ciezarowka = getpid();
        semafor_v(sem, SEMAFOR_TASMA);
        
        snprintf(buf, sizeof(buf),"Ciezarowka %d (PID %d) zajela stanowisko.\n", id, getpid());
        log_write(buf);
        g_odjedz_niepelna = 0;
        int p4_odpowiedzial = 0;

        wyslij_msg_ciezarowka(kolejka, getpid());
    
        while (!g_odjedz_niepelna && !g_zakoncz_prace && !g_zakoncz_przyjmowanie) {
            semafor_p(sem, SEMAFOR_EXPRESS);
            
            if (okienko->gotowe && 
                (okienko->ciezarowka_pid == getpid() || okienko->ciezarowka_pid == 0)) {
                if (okienko->ciezarowka_pid == 0) {
                    okienko->ciezarowka_pid = getpid();
                }
                
                snprintf(buf, sizeof(buf),"Ciezarowka %d: Odbieram %d paczek ekspresowych.\n", 
                     id, okienko->ilosc);
                log_write(buf);
                
                int odebrane = 0;

                while (okienko->ilosc > 0) {
                    Paczka p = okienko->paczki[okienko->ilosc - 1];
                    
                    if (waga + p.waga <= max_waga && pojemnosc + p.objetosc <= max_pojemnosc) {
                        
                        okienko->ilosc--;
                        waga += p.waga;
                        pojemnosc += p.objetosc;
                        liczba_paczek++;
                        ekspres++;
                        odebrane++;
                        
                        snprintf(buf, sizeof(buf),"Ciezarowka %d: +EKSPRES ID %d (%.3f kg). [%.3f/%d kg, %.4f/%d m3]\n",
                             id, p.id, p.waga, waga, max_waga, pojemnosc, max_pojemnosc);
                        log_write(buf);
                    } else {
                        break;
                    }
                }
                
                if (okienko->ilosc == 0) {
                    okienko->gotowe = 0;
                    okienko->ciezarowka_pid = 0;
                    wyslij_msg_odpowiedz(kolejka, 1, 0);
                    p4_odpowiedzial = 1;
                    snprintf(buf, sizeof(buf),"Ciezarowka %d: Odebrano wszystkie %d paczek ekspres.\n",
                         id, odebrane);
                    log_write(buf);
                } else {
                    okienko->ciezarowka_pid = 0;
                    wyslij_msg_odpowiedz(kolejka, 0, okienko->ilosc);
                    p4_odpowiedzial = 1;
                    snprintf(buf, sizeof(buf),"Ciezarowka %d: Odebrano %d ekspres, brak miejsca na reszte (%d).\n",
                         id, odebrane, okienko->ilosc);
                    log_write(buf);
                    semafor_v(sem, SEMAFOR_EXPRESS);
                    break;
                }
            }
            
            semafor_v(sem, SEMAFOR_EXPRESS);
            if (g_odjedz_niepelna || g_zakoncz_prace || g_zakoncz_przyjmowanie) {
                if (g_odjedz_niepelna) {
                    snprintf(buf, sizeof(buf),"Ciezarowka %d: Sygnal - odjade niepelna!\n", id);
                    log_write(buf);
                }
                break;
            }
            
            if (!semafor_p(sem, SEMAFOR_PACZKI)) {
                break;
            }
            
            if (g_odjedz_niepelna || g_zakoncz_prace || g_zakoncz_przyjmowanie) {
                semafor_v(sem, SEMAFOR_PACZKI);
                break;
            }
            
            semafor_p(sem, SEMAFOR_EXPRESS);
            if (okienko->gotowe && okienko->ciezarowka_pid == 0 && okienko->ilosc > 0) {
                semafor_v(sem, SEMAFOR_EXPRESS);
                semafor_v(sem, SEMAFOR_PACZKI);  
                continue;
            }
            semafor_v(sem, SEMAFOR_EXPRESS);
            
            semafor_p(sem, SEMAFOR_TASMA);
            
            if (tasma->aktualna_ilosc == 0) {
                semafor_v(sem, SEMAFOR_TASMA);
                semafor_v(sem, SEMAFOR_PACZKI);
                continue;
            }
            
            Paczka paczka = tasma->bufor[tasma->tail];
            
            if (waga + paczka.waga <= max_waga && pojemnosc + paczka.objetosc <= max_pojemnosc) {
                
                waga += paczka.waga;
                pojemnosc += paczka.objetosc;
                tasma->aktualna_waga -= paczka.waga;
                tasma->aktualna_ilosc--;
                tasma->tail = (tasma->tail + 1) % tasma->max_pojemnosc;
                liczba_paczek++;
                
                snprintf(buf, sizeof(buf),"Ciezarowka %d: +Paczka ID %d (%.3f kg). [%.3f/%d kg, %.4f/%d m3]\n",
                     id, paczka.id, paczka.waga, waga, max_waga, pojemnosc, max_pojemnosc);
                log_write(buf);

                semafor_v(sem, SEMAFOR_TASMA);
                semafor_v(sem, SEMAFOR_WOLNE_MIEJSCA);
            } else {
                semafor_v(sem, SEMAFOR_TASMA);
                semafor_v(sem, SEMAFOR_PACZKI);
                snprintf(buf, sizeof(buf),"Ciezarowka %d: Pelna (%.3f/%d kg, %.4f/%dm3, %d paczek) - odjezdza.\n", 
                     id, waga, max_waga, pojemnosc, max_pojemnosc, liczba_paczek);
                log_write(buf);
                break;
            }
        }
        semafor_p(sem, SEMAFOR_EXPRESS);
        
        if (okienko->gotowe && 
            (okienko->ciezarowka_pid == getpid() || okienko->ciezarowka_pid == 0) &&
            okienko->ilosc > 0) {
            
            if (okienko->ciezarowka_pid == 0) {
                okienko->ciezarowka_pid = getpid();
            }
            
            while (okienko->ilosc > 0) {
                Paczka p = okienko->paczki[okienko->ilosc - 1];
                if (waga + p.waga <= max_waga && pojemnosc + p.objetosc <= max_pojemnosc) {
                    okienko->ilosc--;
                    waga += p.waga;
                    pojemnosc += p.objetosc;
                    liczba_paczek++;
                    ekspres++;
                } else {
                    break;
                }
            }

            if (okienko->ilosc == 0) {
                okienko->gotowe = 0;
                okienko->ciezarowka_pid = 0;
                if (!p4_odpowiedzial) wyslij_msg_odpowiedz(kolejka, 1, 0);
            } else {
                okienko->ciezarowka_pid = 0;
                if (!p4_odpowiedzial) wyslij_msg_odpowiedz(kolejka, 0, okienko->ilosc);
            }
            p4_odpowiedzial = 1;
        }
        semafor_v(sem, SEMAFOR_EXPRESS);
        semafor_p(sem, SEMAFOR_TASMA);
        tasma->ciezarowka = 0;
        semafor_v(sem, SEMAFOR_TASMA);
        semafor_v(sem, SEMAFOR_CIEZAROWKI);
        
        if (!p4_odpowiedzial) wyslij_msg_odpowiedz(kolejka, 1, 0);

        if (liczba_paczek > 0) {
            if (g_odjedz_niepelna) {
                snprintf(buf, sizeof(buf), 
                    "Ciezarowka %d: ODJAZD (SIGUSR1 - niepelna) [%d paczek (%d EKSPRES), %.3f/%dkg, %.4f/%dm3]\n",
                    id, liczba_paczek, ekspres, waga, max_waga, pojemnosc, max_pojemnosc);
            } else {
                snprintf(buf, sizeof(buf), 
                    "Ciezarowka %d: ODJAZD [%d paczek (%d EKSPRES), %.3f/%dkg, %.4f/%dm3]\n",
                    id, liczba_paczek, ekspres, waga, max_waga, pojemnosc, max_pojemnosc);
            }
            log_write(buf);
            
            // time_t czas = czas_rozwozu;
            // while (czas > 0 && !g_zakoncz_prace) {
            //     czas = sleep(czas); 
            // }
            
            snprintf(buf, sizeof(buf), "Ciezarowka %d: Wraca po rozwozie\n", id);
            log_write(buf);
            
            waga = pojemnosc = 0;
            liczba_paczek = ekspres = 0;
        }

        if (g_zakoncz_przyjmowanie) {
            semafor_p(sem, SEMAFOR_TASMA);
            int zostalo = tasma->aktualna_ilosc;
            semafor_v(sem, SEMAFOR_TASMA);
            if (zostalo == 0) break;
        }
    }
    
    if (liczba_paczek > 0) {
        snprintf(buf, sizeof(buf), 
        "Ciezarowka %d: OSTATNI KURS [%d paczek, %.3fkg, %.4fm3]\n", 
            id, liczba_paczek, waga, pojemnosc);
        log_write(buf);
        // time_t czas = czas_rozwozu;
        // while (czas > 0 && !g_zakoncz_prace) czas = sleep(czas);
    }

    shmdt(tasma);
    shmdt(okienko);
    snprintf(buf, sizeof(buf),"Ciezarowka %d zakonczyla prace.\n", id);
    log_write(buf);
    sem_log_close();
    log_close();
    return 0;
}