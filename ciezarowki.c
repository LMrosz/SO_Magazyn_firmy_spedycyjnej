#include "utils.h"

int main(int argc, char *argv[]) {
    srand(time(NULL) ^ getpid());
    
    if (argc < 9) {
        fprintf(stderr, "Uzycie: %s id shmid_tasma semafor waga pojemnosc czas shmid_okienko log_dir\n", argv[0]);
        return 1;
    }
    
    int id_ciezarowki = atoi(argv[1]);
    int shmid_tasma = atoi(argv[2]);
    int semafor = atoi(argv[3]);
    int dopuszczalna_waga = atoi(argv[4]);
    int dopuszczalna_pojemnosc = atoi(argv[5]);
    time_t czas_rozwozu = atoi(argv[6]);
    int shmid_okienko = atoi(argv[7]);
    strncpy(g_log_dir, argv[8], sizeof(g_log_dir) - 1);
    
    ustaw_handlery_ciezarowka();
    
    log_init(semafor, "ciezarowki.log", COL_GREEN);
    sem_log_init();
    char buf[256];
    
    double waga = 0;
    double pojemnosc = 0;
    int liczba_paczek_c = 0;
    int liczba_ekspres = 0;
    
    Tasma *tasma = (Tasma *)shmat(shmid_tasma, NULL, 0);
    if (tasma == (Tasma *)(-1)) {
        perror("shmat tasma");
        return 1;
    }
    
    OkienkoEkspresShm *okienko = (OkienkoEkspresShm *)shmat(shmid_okienko, NULL, 0);
    if (okienko == (OkienkoEkspresShm *)(-1)) {
        perror("shmat okienko");
        shmdt(tasma);
        return 1;
    }

    snprintf(buf, sizeof(buf), "Ciezarowka %d (PID %d) rozpoczyna prace.\n", id_ciezarowki, getpid());
    log_write(buf);

    while (!g_zakoncz_prace) {
        if (g_zakoncz_przyjmowanie) {
            semafor_p(semafor, SEMAFOR_TASMA);
            int paczki_na_tasmie = tasma->aktualna_ilosc;
            semafor_v(semafor, SEMAFOR_TASMA);
            
            if (paczki_na_tasmie == 0 && liczba_paczek_c == 0) {
                snprintf(buf, sizeof(buf),"Ciezarowka %d: Brak paczek - koncze.\n", id_ciezarowki);
                log_write(buf);
                break;
            }
        }
        
        semafor_p(semafor, SEMAFOR_CIEZAROWKI);
        if (g_zakoncz_prace || g_zakoncz_przyjmowanie) {
            semafor_v(semafor, SEMAFOR_CIEZAROWKI);
            break;
        }
        
        semafor_p(semafor, SEMAFOR_TASMA);
        tasma->ciezarowka = getpid();
        semafor_v(semafor, SEMAFOR_TASMA);
        
        snprintf(buf, sizeof(buf),"Ciezarowka %d (PID %d) zajela stanowisko.\n", id_ciezarowki, getpid());
        log_write(buf);
        g_odjedz_niepelna = 0;

        semafor_v(semafor, SEMAFOR_P4_CZEKA);
    
        while (!g_odjedz_niepelna && !g_zakoncz_prace) {
            semafor_p(semafor, SEMAFOR_EXPRESS);
            
            if (okienko->gotowe && 
                (okienko->ciezarowka_pid == getpid() || okienko->ciezarowka_pid == 0) &&
                okienko->ilosc > 0) {
                if (okienko->ciezarowka_pid == 0) {
                    okienko->ciezarowka_pid = getpid();
                }
                
                snprintf(buf, sizeof(buf),"Ciezarowka %d: Odbieram %d paczek ekspresowych.\n", 
                     id_ciezarowki, okienko->ilosc);
                log_write(buf);
                
                int odebrane = 0;

                while (okienko->ilosc > 0) {
                    Paczka p = okienko->paczki[okienko->ilosc - 1];
                    
                    if (waga + p.waga <= dopuszczalna_waga &&
                        czy_pojemnosc(p.typ, dopuszczalna_pojemnosc - pojemnosc)) {
                        
                        okienko->ilosc--;
                        waga += p.waga;
                        pojemnosc += p.objetosc;
                        liczba_paczek_c++;
                        liczba_ekspres++;
                        odebrane++;
                        
                        snprintf(buf, sizeof(buf),"Ciezarowka %d: +EKSPRES ID %d (%.2f kg). [%.2f/%d kg, %.2f/%d m3]\n",
                             id_ciezarowki, p.id, p.waga, waga, dopuszczalna_waga, pojemnosc, dopuszczalna_pojemnosc);
                        log_write(buf);
                    } else {
                        break;
                    }
                }
                
                if (okienko->ilosc == 0) {
                    okienko->gotowe = 0;
                    okienko->ciezarowka_pid = 0;
                    snprintf(buf, sizeof(buf),"Ciezarowka %d: Odebrano wszystkie %d paczek ekspres.\n",
                         id_ciezarowki, odebrane);
                    log_write(buf);
                } else {
                    snprintf(buf, sizeof(buf),"Ciezarowka %d: Odebrano %d ekspres, brak miejsca na reszte (%d).\n",
                         id_ciezarowki, odebrane, okienko->ilosc);
                    log_write(buf);
                    okienko->ciezarowka_pid = 0;
                }
            }
            
            semafor_v(semafor, SEMAFOR_EXPRESS);
            semafor_p(semafor, SEMAFOR_TASMA);
            int jest_paczka = (tasma->aktualna_ilosc > 0);
            Paczka nastepna;
            if (jest_paczka) {
                nastepna = tasma->bufor[tasma->tail];
            }
            semafor_v(semafor, SEMAFOR_TASMA);
            
            if (jest_paczka) {
                if (waga + nastepna.waga > dopuszczalna_waga ||
                    !czy_pojemnosc(nastepna.typ, dopuszczalna_pojemnosc - pojemnosc)) {
                    snprintf(buf, sizeof(buf),"Ciezarowka %d: Pelna (%.2f/%d kg, %.2f/%d m3) - odjezdza.\n", 
                         id_ciezarowki, waga, dopuszczalna_waga, pojemnosc, dopuszczalna_pojemnosc);
                    log_write(buf);
                    break;
                }
            }

            if (g_odjedz_niepelna || g_zakoncz_prace || g_zakoncz_przyjmowanie) {
                if (g_odjedz_niepelna) {
                    snprintf(buf, sizeof(buf),"Ciezarowka %d: Sygnal - odjade niepelna!\n", id_ciezarowki);
                    log_write(buf);
                }
                break;
            }
            
            semafor_p(semafor, SEMAFOR_PACZKI);
            
            if (g_odjedz_niepelna || g_zakoncz_prace || g_zakoncz_przyjmowanie) {
                semafor_v(semafor, SEMAFOR_PACZKI);
                break;
            }
            
            if (g_zakoncz_przyjmowanie) {
                semafor_p(semafor, SEMAFOR_TASMA);
                if (tasma->aktualna_ilosc == 0) {
                    semafor_v(semafor, SEMAFOR_TASMA);
                    semafor_v(semafor, SEMAFOR_PACZKI);
                    break;
                }
                semafor_v(semafor, SEMAFOR_TASMA);
            }
            
            semafor_p(semafor, SEMAFOR_TASMA);
            
            if (tasma->aktualna_ilosc == 0) {
                semafor_v(semafor, SEMAFOR_TASMA);
                semafor_v(semafor, SEMAFOR_PACZKI);
                continue;
            }
            
            Paczka paczka = tasma->bufor[tasma->tail];
            
            if (waga + paczka.waga <= dopuszczalna_waga &&
                czy_pojemnosc(paczka.typ, dopuszczalna_pojemnosc - pojemnosc)) {
                
                waga += paczka.waga;
                pojemnosc += paczka.objetosc;
                tasma->aktualna_waga -= paczka.waga;
                tasma->aktualna_ilosc--;
                tasma->tail = (tasma->tail + 1) % tasma->max_pojemnosc;
                liczba_paczek_c++;
                
                snprintf(buf, sizeof(buf),"Ciezarowka %d: +Paczka ID %d (%.2f kg). [%.2f/%d kg, %.2f/%d m3]\n",
                     id_ciezarowki, paczka.id, paczka.waga, waga, dopuszczalna_waga, pojemnosc, dopuszczalna_pojemnosc);
                log_write(buf);

                semafor_v(semafor, SEMAFOR_WOLNE_MIEJSCA);
                semafor_v(semafor, SEMAFOR_TASMA);
            } else {
                semafor_v(semafor, SEMAFOR_TASMA);
                semafor_v(semafor, SEMAFOR_PACZKI);
                snprintf(buf, sizeof(buf),"Ciezarowka %d: Pelna (%.2f/%d kg) - odjezdza.\n", 
                     id_ciezarowki, waga, dopuszczalna_waga);
                log_write(buf);
                break;
            }
        }
        semafor_p(semafor, SEMAFOR_EXPRESS);
        
        if (okienko->gotowe && 
            (okienko->ciezarowka_pid == getpid() || okienko->ciezarowka_pid == 0) &&
            okienko->ilosc > 0) {
            
            if (okienko->ciezarowka_pid == 0) {
                okienko->ciezarowka_pid = getpid();
            }
            
            snprintf(buf, sizeof(buf),"Ciezarowka %d: Przed odjazdem - odbieram %d ekspres.\n", 
                 id_ciezarowki, okienko->ilosc);
            log_write(buf);
            
            int odebrane = 0;
            while (okienko->ilosc > 0) {
                Paczka p = okienko->paczki[okienko->ilosc - 1];
                
                if (waga + p.waga <= dopuszczalna_waga &&
                    czy_pojemnosc(p.typ, dopuszczalna_pojemnosc - pojemnosc)) {
                    
                    okienko->ilosc--;
                    waga += p.waga;
                    pojemnosc += p.objetosc;
                    liczba_paczek_c++;
                    liczba_ekspres++;
                    odebrane++;
                } else {
                    break;
                }
            }
            
            if (okienko->ilosc == 0) {
                okienko->gotowe = 0;
                okienko->ciezarowka_pid = 0;
            } else {
                okienko->ciezarowka_pid = 0;
            }
            
            if (odebrane > 0) {
                snprintf(buf, sizeof(buf),"Ciezarowka %d: Dodano %d ekspres przed odjazdem.\n", 
                     id_ciezarowki, odebrane);
                log_write(buf);
            }
        }
        
        semafor_v(semafor, SEMAFOR_EXPRESS);
        semafor_p(semafor, SEMAFOR_TASMA);
        tasma->ciezarowka = 0;
        semafor_v(semafor, SEMAFOR_TASMA);
        semafor_v(semafor, SEMAFOR_CIEZAROWKI);
        if (liczba_paczek_c > 0) {
            snprintf(buf, sizeof(buf),"Ciezarowka %d ODJEZDZA: %.2f/%d kg, %d paczek (%d EKSPRES).\n",
                 id_ciezarowki, waga, dopuszczalna_waga, liczba_paczek_c, liczba_ekspres);
            log_write(buf);
            
            time_t czas = czas_rozwozu;
            while (czas > 0 && !g_zakoncz_prace) {
                czas = sleep(czas);
            }
            
            snprintf(buf, sizeof(buf),"Ciezarowka %d: Rozwoz zakonczony (%d paczek). Wraca.\n", 
                 id_ciezarowki, liczba_paczek_c);
            log_write(buf);

            waga = 0;
            pojemnosc = 0;
            liczba_paczek_c = 0;
            liczba_ekspres = 0;
        }
        
        if (g_zakoncz_przyjmowanie) {
            semafor_p(semafor, SEMAFOR_TASMA);
            int pozostalo = tasma->aktualna_ilosc;
            semafor_v(semafor, SEMAFOR_TASMA);
            
            if (pozostalo == 0) {
                break;
            }
        }
    }

    if (liczba_paczek_c > 0) {
        snprintf(buf, sizeof(buf),"Ciezarowka %d: OSTATNI KURS z %d paczkami (%d EKSPRES)...\n", 
             id_ciezarowki, liczba_paczek_c, liczba_ekspres);
        log_write(buf);
        sleep(czas_rozwozu);
    }
    
    shmdt(tasma);
    shmdt(okienko);
    snprintf(buf, sizeof(buf),"Ciezarowka %d zakonczyla prace.\n", id_ciezarowki);
    log_write(buf);
    sem_log_close();
    log_close();
    return 0;
}