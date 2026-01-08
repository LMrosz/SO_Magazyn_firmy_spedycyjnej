#include "utils.h"

int main(int argc, char *argv[]) {
    srand(time(NULL) ^ getpid());
    
    if (argc < 8) {
        fprintf(stderr, "Uzycie: %s id shmid_tasma semafor waga pojemnosc czas shmid_okienko\n", argv[0]);
        return 1;
    }
    
    int id_ciezarowki = atoi(argv[1]);
    int shmid_tasma = atoi(argv[2]);
    int semafor = atoi(argv[3]);
    int dopuszczalna_waga = atoi(argv[4]);
    int dopuszczalna_pojemnosc = atoi(argv[5]);
    time_t czas_rozwozu = atoi(argv[6]);
    int shmid_okienko = atoi(argv[7]);
    
    ustaw_handlery_ciezarowka();
    otworz_plik_wyniki(semafor);
    
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
    
    while (!g_zakoncz_prace) {
        if (g_zakoncz_przyjmowanie) {
            semafor_p(semafor, SEMAFOR_TASMA);
            int paczki_na_tasmie = tasma->aktualna_ilosc;
            semafor_v(semafor, SEMAFOR_TASMA);
            
            if (paczki_na_tasmie == 0 && liczba_paczek_c == 0) {
                logi("Ciezarowka %d: Brak paczek do rozwiezienia - koncze.\n", id_ciezarowki);
                break;
            }
        }
        
        semafor_p(semafor, SEMAFOR_CIEZAROWKI);
        if (g_zakoncz_prace) {
            semafor_v(semafor, SEMAFOR_CIEZAROWKI);
            break;
        }
        
        semafor_p(semafor, SEMAFOR_TASMA);
        tasma->ciezarowka = getpid();
        semafor_v(semafor, SEMAFOR_TASMA);
        
        logi("Ciezarowka %d (PID %d) zajela stanowisko.\n", id_ciezarowki, getpid());
        g_odjedz_niepelna = 0;
        
        while (!g_odjedz_niepelna && !g_zakoncz_prace) {
            semafor_p(semafor, SEMAFOR_EXPRESS);
            
            if (okienko->gotowe && okienko->ciezarowka_pid == getpid()) {
                logi("Ciezarowka %d: Odbieram %d paczek ekspresowych z okienka...\n", 
                     id_ciezarowki, okienko->ilosc);
                
                int odebrane = 0;
                int nieodebrane = 0;

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
                        
                        logi("Ciezarowka %d: Odebral EKSPRES ID %d (%.2f kg). Ladunek: %.2f/%d kg\n",
                             id_ciezarowki, p.id, p.waga, waga, dopuszczalna_waga);
                    } else {
                        nieodebrane = okienko->ilosc;
                        break;
                    }
                }
                
                if (okienko->ilosc == 0) {
                    okienko->gotowe = 0;
                    okienko->ciezarowka_pid = 0;
                    logi("Ciezarowka %d: Odebrano wszystkie %d paczek ekspres z okienka.\n",
                         id_ciezarowki, odebrane);
                } else {
                    logi("Ciezarowka %d: Odebrano %d ekspres, pozostalo %d (brak miejsca).\n",
                         id_ciezarowki, odebrane, nieodebrane);
                    okienko->ciezarowka_pid = 0;
                    okienko->gotowe = 1;
                }
            }
            
            semafor_v(semafor, SEMAFOR_EXPRESS);
            semafor_p(semafor, SEMAFOR_PACZKI);
            
            if (g_odjedz_niepelna || g_zakoncz_prace) {
                semafor_v(semafor, SEMAFOR_PACZKI);
                if (g_odjedz_niepelna) {
                    logi("Ciezarowka %d: Otrzymano sygnal - odjade z niepelnym ladunkiem!\n", id_ciezarowki);
                }
                break;
            }
            
            if (g_zakoncz_przyjmowanie) {
                semafor_p(semafor, SEMAFOR_TASMA);
                if (tasma->aktualna_ilosc == 0) {
                    semafor_v(semafor, SEMAFOR_TASMA);
                    semafor_v(semafor, SEMAFOR_PACZKI);
                    logi("Ciezarowka %d: Tasma pusta, koncze ladowanie.\n", id_ciezarowki);
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
                
                logi("Ciezarowka %d: Zaladowal paczke ID %d (%.2f kg). Ladunek: %.2f/%d kg\n",
                     id_ciezarowki, paczka.id, paczka.waga, waga, dopuszczalna_waga);
                
                semafor_v(semafor, SEMAFOR_WOLNE_MIEJSCA);
                semafor_v(semafor, SEMAFOR_TASMA);
                
            } else {
                semafor_v(semafor, SEMAFOR_TASMA);
                semafor_v(semafor, SEMAFOR_PACZKI);
                logi("Ciezarowka %d: Pelna (%.2f/%d kg) - odjezdza.\n", 
                     id_ciezarowki, waga, dopuszczalna_waga);
                break;
            }
        }
        
        semafor_p(semafor, SEMAFOR_TASMA);
        tasma->ciezarowka = 0;
        semafor_v(semafor, SEMAFOR_TASMA);
        semafor_v(semafor, SEMAFOR_CIEZAROWKI);
        
        if (liczba_paczek_c > 0) {
            if (g_odjedz_niepelna) {
                logi("Ciezarowka %d ODJEZDZA NIEPELNA: %.2f/%d kg, %d paczek (%d EKSPRES)\n",
                     id_ciezarowki, waga, dopuszczalna_waga, liczba_paczek_c, liczba_ekspres);
            } else {
                logi("Ciezarowka %d ODJEZDZA PELNA: %.2f/%d kg, %d paczek (%d EKSPRES). Rozwoz: %ld s.\n",
                     id_ciezarowki, waga, dopuszczalna_waga, liczba_paczek_c, liczba_ekspres, czas_rozwozu);
            }
            
            time_t czas = czas_rozwozu;
            while (czas > 0) {
                czas = sleep(czas);
                if (g_zakoncz_prace) {
                    break;
                }
            }
            
            logi("Ciezarowka %d: Rozwoz zakonczony, rozwiozla %d paczek. Wraca pusta.\n", 
                 id_ciezarowki, liczba_paczek_c);
            
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
                logi("Ciezarowka %d: Wszystko rozwiezione - koncze prace.\n", id_ciezarowki);
                break;
            }
        }
    }

    if (liczba_paczek_c > 0) {
        logi("Ciezarowka %d: OSTATNI KURS z %d paczkami...\n", id_ciezarowki, liczba_paczek_c);
        sleep(czas_rozwozu);
        logi("Ciezarowka %d: Ostatni kurs zakonczony.\n", id_ciezarowki);
    }
    
    shmdt(tasma);
    shmdt(okienko);
    logi("Ciezarowka %d zakonczyla prace.\n", id_ciezarowki);
    zamknij_plik_wyniki();
    return 0;
}