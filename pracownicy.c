#include "utils.h"

static int g_paczek_na_ture = 0;
static int g_interwal = 10;
static int g_procent_ekspres = 25;

int main(int argc, char *argv[]) {
    srand(time(NULL) ^ getpid());
    
    if (argc < 11) {
        fprintf(stderr, "Uzycie: %s id sem shm_tasma shm_okienko log_dir kolejka paczki interwal shm_licznik procent\n", argv[0]);
        return 1;
    }
    
    int id = atoi(argv[1]);
    int sem = atoi(argv[2]);
    int shm_tasma = atoi(argv[3]);
    int shm_okienko = atoi(argv[4]);
    strncpy(g_log_dir, argv[5], sizeof(g_log_dir) - 1);
    int kolejka = atoi(argv[6]);
    g_paczek_na_ture = atoi(argv[7]);
    g_interwal = atoi(argv[8]);
    int shm_licznik = atoi(argv[9]);
    g_procent_ekspres = atoi(argv[10]);
    
    ustaw_handlery_pracownik(id);
    log_init(sem, "pracownicy.log", COL_CYAN);
    sem_log_init();
    
    Tasma *tasma = (Tasma *)shmat(shm_tasma, NULL, 0);
    if (tasma == (Tasma *)(-1)) {
        log_error("Blad dostepu do pamieci dzielonej tasmy");
        return 1;
    }
    
    OkienkoEkspresShm *okienko = (OkienkoEkspresShm *)shmat(shm_okienko, NULL, 0);
    if (okienko == (OkienkoEkspresShm *)(-1)) {
        log_error("Blad dostepu do pamieci dzielonej pomiedzy P4 a ciezarowka");
        shmdt(tasma);
        return 1;
    }
    
    LicznikId *licznik = (LicznikId *)shmat(shm_licznik, NULL, 0);
    if (licznik == (LicznikId *)(-1)) {
        log_error("Blad dostepu do pamieci dzielonej licznika");
        shmdt(tasma);
        shmdt(okienko);
        return 1;
    }

    char buf[256];

    if (id == 4) {
        snprintf(buf, sizeof(buf),"Pracownik P4 (PID %d) - dostawa paczek ekspres.\n", getpid());
        log_write(buf);
        snprintf(buf, sizeof(buf),"P4: Czekam na sygnal SIGUSR2 od dyspozytora aby dostarczyc paczki.\n");
        log_write(buf);

        int pojemnosc_tablicy = 100;
        Paczka *paczki_ekspres = malloc(pojemnosc_tablicy * sizeof(Paczka));
        if (!paczki_ekspres) {
            log_error("Blad dynamicznej alokacji pamieci tablicy paczek express w P4");
            shmdt(licznik);
            shmdt(tasma);
            shmdt(okienko);
            return 1;
        }
        int ile_oczekujacych = 0;
        double suma_waga = 0, suma_obj = 0;
        time_t ostatnie_generowanie = 0;
        
        while (!g_zakoncz_prace){
            time_t teraz = time(NULL);
            if(licznik->generowanie_aktywne && g_paczek_na_ture > 0 && (teraz - ostatnie_generowanie) >= g_interwal){

                int do_wygenerowania = (g_paczek_na_ture * g_procent_ekspres) / 100;
                if (g_procent_ekspres == 0) {
                    ostatnie_generowanie = teraz;
                    continue;
                }
                if (do_wygenerowania < 1) do_wygenerowania = 1;

                if (ile_oczekujacych + do_wygenerowania > pojemnosc_tablicy){
                    pojemnosc_tablicy = ile_oczekujacych + do_wygenerowania + 50;
                    Paczka *nowa = realloc(paczki_ekspres, pojemnosc_tablicy *sizeof(Paczka));
                    if (!nowa){
                        log_error("P4: Blad realokacji pamieci!\n");
                        continue;
                    }
                    paczki_ekspres = nowa;
                }

                for (int i = 0; i < do_wygenerowania && !g_zakoncz_prace; i++) {
                    int id = pobierz_nastepne_id(sem, licznik);
                    Paczka p = generuj_paczke_ekspres(id);
                    paczki_ekspres[ile_oczekujacych++] = p;
                    suma_waga += p.waga;
                    suma_obj += p.objetosc;
                    
                    snprintf(buf, sizeof(buf), "P4: Wygenerowano EKSPRES ID %d (%.3f kg, typ %s)\n",
                             p.id, p.waga, nazwa_typu(p.typ));
                    log_write(buf);
                }

                snprintf(buf, sizeof(buf), "P4: Wygenerowano %d paczek EKSPRES. Bufor: %d paczek.\n",
                         do_wygenerowania, ile_oczekujacych);
                log_write(buf);
                ostatnie_generowanie = teraz;
            }   

            if(!g_dostarcz_ekspres){
                if(!licznik->generowanie_aktywne && ile_oczekujacych == 0){
                    log_write("P4: Brak paczek ekspres - koncze.\n");
                    break;
                }
                //Testy - do usuniecia
                if(g_procent_ekspres >= 100 && ile_oczekujacych > 0){
                    snprintf(buf, sizeof(buf), "P4: Tryb 100%% EXPRESS - auto-dostawa %d paczek.\n", ile_oczekujacych);
                    log_write(buf);
                    g_dostarcz_ekspres = 1;
                } else {
                    // usleep(500000);
                    continue;
                }
            }

            g_dostarcz_ekspres = 0;

            if(ile_oczekujacych == 0){
                log_write("P4: Otrzymano sygnal SIGUSR2, ale brak paczek ekspresowych!\n");
                continue;
            }

            snprintf(buf, sizeof(buf),"P4: Otrzymano SIGUSR2 - dostarczam %d paczek EKSPRES!\n", ile_oczekujacych);
            log_write(buf);

            snprintf(buf, sizeof(buf),"P4: Czekam na ciezarowke przy tasmie...\n");
            log_write(buf);

            while (ile_oczekujacych > 0 && !g_zakoncz_prace) {
                MsgCiezarowkaPrzyTasmie msg;
                if (!odbierz_msg_ciezarowka(kolejka, &msg)) {
                    if (g_zakoncz_prace) break;
                    continue;
                }
                
                if (msg.ciezarowka_pid == 0) continue;
                
                snprintf(buf, sizeof(buf), 
                    "P4: Ciezarowka zgloszona - przekazuje %d paczek (%.2fkg, %.4fm3)\n", 
                    ile_oczekujacych, suma_waga, suma_obj);
                log_write(buf);
                (void)msg; 
                
                semafor_p(sem, SEMAFOR_EXPRESS);
                okienko->ciezarowka_pid = 0; 
                okienko->ilosc = 0;
                for (int i = 0; i < ile_oczekujacych; i++) {
                    okienko->paczki[okienko->ilosc++] = paczki_ekspres[i];
                }
                okienko->gotowe = 1;
                semafor_v(sem, SEMAFOR_EXPRESS);
                
                for (int i = 0; i < 5; i++) {
                    semafor_v(sem, SEMAFOR_PACZKI);
                }
                
                MsgP4Odpowiedz odp;
                if (!odbierz_msg_odpowiedz(kolejka, &odp)) break;
                
                semafor_p(sem, SEMAFOR_EXPRESS);
                int zostalo = okienko->ilosc;
                
                if (zostalo == 0) {
                    semafor_v(sem, SEMAFOR_EXPRESS);
                    log_write("P4: Ciezarowka odebrala wszystko!\n");
                    ile_oczekujacych = 0;
                    suma_waga = suma_obj = 0;
                } else {
                    ile_oczekujacych = 0;
                    suma_waga = suma_obj = 0;
                    for (int i = 0; i < okienko->ilosc; i++) {
                        paczki_ekspres[ile_oczekujacych++] = okienko->paczki[i];
                        suma_waga += okienko->paczki[i].waga;
                        suma_obj += okienko->paczki[i].objetosc;
                    }
                    okienko->ilosc = 0;
                    okienko->gotowe = 0;
                    okienko->ciezarowka_pid = 0;
                    semafor_v(sem, SEMAFOR_EXPRESS);
                    
                    snprintf(buf, sizeof(buf), 
                        "P4: Zostalo %d paczek (%.2fkg, %.4fm3) - szukam nastepnej ciezarowki\n", 
                        ile_oczekujacych, suma_waga, suma_obj);
                    log_write(buf);
                }
            }
            
            if (ile_oczekujacych == 0) {
                log_write("P4: Wszystkie paczki dostarczone - kontynuuje generowanie.\n");
            }
        }
        
        free(paczki_ekspres);
        log_write("P4: Koncze prace\n");
        
    } else {
        snprintf(buf, sizeof(buf), "P%d (PID %d) start - ZWYKLE\n", id, getpid());
        log_write(buf);
        
        time_t ostatnie_generowanie = 0;

        while (!g_zakoncz_prace) {
            if (!licznik->generowanie_aktywne) {
                snprintf(buf, sizeof(buf), "P%d: Generowanie wylaczone - koncze\n", id);
                log_write(buf);
                break;
            }
            
            time_t teraz = time(NULL);
            if ((teraz - ostatnie_generowanie) < g_interwal) {
                // usleep(100000); 
                continue;
            }
            
            int paczki_zwykle = (g_paczek_na_ture * (100 - g_procent_ekspres)) / 100;
            if (g_procent_ekspres >= 100) {
                // usleep(500000);
                continue;
            }
            if (paczki_zwykle < 1) paczki_zwykle = 1;
            int do_gen = paczki_zwykle / 3;
            if (do_gen < 1) do_gen = 1;
            
            for (int i = 0; i < do_gen && !g_zakoncz_prace; i++) {
                int pid = pobierz_nastepne_id(sem, licznik);
                Paczka p = generuj_paczke_zwykla(pid);
                
                snprintf(buf, sizeof(buf), 
                    "P%d: Wygenerowano ZWYKLA ID=%d typ=%s waga=%.3fkg obj=%.4fm3\n",
                    id, p.id, nazwa_typu(p.typ), p.waga, p.objetosc);
                log_write(buf);
                
                if (!semafor_p(sem, SEMAFOR_WOLNE_MIEJSCA)) break;
                if (g_zakoncz_prace) { semafor_v(sem, SEMAFOR_WOLNE_MIEJSCA); break; }
                
                semafor_p(sem, SEMAFOR_TASMA);
                while (tasma->aktualna_waga + p.waga > tasma->max_waga && !g_zakoncz_prace) {
                    snprintf(buf, sizeof(buf), 
                        "P%d: Tasma przeciazona (%.2f/%dkg) - czekam\n",
                        id, tasma->aktualna_waga, tasma->max_waga);
                    log_write(buf);
                    semafor_v(sem, SEMAFOR_TASMA);
                    semafor_v(sem, SEMAFOR_WOLNE_MIEJSCA);
                    // usleep(50000);
                    if (!semafor_p(sem, SEMAFOR_WOLNE_MIEJSCA)) goto koniec_petli;
                    if (g_zakoncz_prace) { semafor_v(sem, SEMAFOR_WOLNE_MIEJSCA); goto koniec_petli; }
                    semafor_p(sem, SEMAFOR_TASMA);
                }
                
                if (g_zakoncz_prace) {
                    semafor_v(sem, SEMAFOR_TASMA);
                    semafor_v(sem, SEMAFOR_WOLNE_MIEJSCA);
                    break;
                }
                tasma->bufor[tasma->head] = p;
                tasma->head = (tasma->head + 1) % tasma->max_pojemnosc;
                tasma->aktualna_ilosc++;
                tasma->aktualna_waga += p.waga;
                
                snprintf(buf, sizeof(buf), 
                    "P%d: Polozyl ID=%d na tasmie (ilosc=%d/%d, waga=%.2f/%dkg)\n",
                    id, p.id, tasma->aktualna_ilosc, tasma->max_pojemnosc,
                    tasma->aktualna_waga, tasma->max_waga);
                log_write(buf);
                semafor_v(sem, SEMAFOR_TASMA);
                semafor_v(sem, SEMAFOR_PACZKI);
            }
            
            ostatnie_generowanie = teraz;
        }
        koniec_petli:
        
        snprintf(buf, sizeof(buf), "P%d: Koncze prace\n", id);
        log_write(buf);
    }
    
    shmdt(tasma);
    shmdt(okienko);
    shmdt(licznik);
    log_close();
    return 0;
}

        
