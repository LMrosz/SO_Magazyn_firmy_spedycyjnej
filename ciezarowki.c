#include "utils.h"

static int wyslij_msg(int kolejka, const void *msg, size_t size, const char *errlog) {
    while (msgsnd(kolejka, msg, size, 0) == -1) {
        if (errno == EINTR)
            continue;
        if (errno == EIDRM || errno == EINVAL) {
            log_write("Ciezarowka: kolejka usunieta - koncze\n");
            g_zakoncz_prace = 1;
            return 0;
        }
        if (errlog)
            log_error(errlog);
        return 0;
    }
    return 1;
}

static int sprawdz_dostawa_express(int kolejka, MsgP4Dostawa *msg) {
    struct msqid_ds info;
    if (msgctl(kolejka, IPC_STAT, &info) == -1) {
        if (errno == EIDRM || errno == EINVAL) {
            g_zakoncz_prace = 1;
        }
        return 0;
    }
    if (info.msg_qnum == 0) {
        return 0;
    }
    ssize_t ret = msgrcv(kolejka, msg, sizeof(*msg) - sizeof(long),
                         MSG_P4_DOSTAWA_GOTOWA, 0);
    return (ret != -1) ? 1 : 0;
}

static int odbierz_express(int sem, OkienkoEkspresShm *okienko, int kolejka, double *waga, double *pojemnosc, int *liczba_paczek, int *ekspres, int max_waga, int max_pojemnosc, int id, char *buf) {
    MsgP4Dostawa msg_przekazane;
    int got_msg = 0;

    while (!g_zakoncz_prace && !g_zakoncz_przyjmowanie && !got_msg) {
        ssize_t ret = msgrcv(kolejka, &msg_przekazane, sizeof(msg_przekazane) - sizeof(long), MSG_P4_PACZKI_PRZEKAZANE, 0);
        if (ret != -1) {
            got_msg = 1;
            break;
        }
        if (errno == EINTR)
            continue;
        return 0;
    }

    if (g_zakoncz_prace || g_zakoncz_przyjmowanie || !got_msg)
        return 0;

    snprintf(buf, 256, "Ciezarowka %d: Odbieram paczki EXPRESS (priorytet!)...\n", id);
    log_write(buf);

    semafor_p(sem, SEMAFOR_EXPRESS);

    int odebrane = 0;
    int nieodebrane = 0;

    while (okienko->ilosc > 0) {
        Paczka p = okienko->paczki[0];

        if (*waga + p.waga <= max_waga && *pojemnosc + p.objetosc <= max_pojemnosc) {
            for (int k = 0; k < okienko->ilosc - 1; k++) {
                okienko->paczki[k] = okienko->paczki[k + 1];
            }
            okienko->ilosc--;
            *waga += p.waga;
            *pojemnosc += p.objetosc;
            (*liczba_paczek)++;
            (*ekspres)++;
            odebrane++;

            snprintf(buf, 256, "Ciezarowka %d: +EXPRESS ID %d (%.3f kg) [%.3f/%d kg, %.4f/%d m3]\n", id, p.id, p.waga, *waga, max_waga, *pojemnosc, max_pojemnosc);
            log_write(buf);
        } else {
            nieodebrane = okienko->ilosc;
            break;
        }
    }

    if (okienko->ilosc == 0) {
        okienko->gotowe = 0;
    }

    semafor_v(sem, SEMAFOR_EXPRESS);

    MsgPotwierdzenie msg_potw = {
        .mtype = MSG_ODEBRANO_POTWIERDZENIE,
        .ile_odebranych = odebrane,
        .ile_zostalo = nieodebrane,
        .pojemnosc_wolna = (int)((max_waga - *waga) / 25.0)
    };
    if (!wyslij_msg(kolejka, &msg_potw, sizeof(msg_potw) - sizeof(long),
                    "Ciezarowka: blad wysylania potwierdzenia\n"))
        return 0;

    snprintf(buf, 256, "Ciezarowka %d: Odebrano %d EXPRESS, zostalo: %d\n", id, odebrane, nieodebrane);
    log_write(buf);

    return (nieodebrane > 0) ? -1 : odebrane;
}

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

    snprintf(buf, sizeof(buf), "Ciezarowka %d (PID %d) [W=%dkg, V=%dm3, Ti=%lds]\n", id, getpid(), max_waga, max_pojemnosc, czas_rozwozu);
    log_write(buf);

    while (!g_zakoncz_prace && !g_zakoncz_przyjmowanie) {
        if (!semafor_p(sem, SEMAFOR_CIEZAROWKI))
            break;

        if (g_zakoncz_prace || g_zakoncz_przyjmowanie) {
            semafor_v(sem, SEMAFOR_CIEZAROWKI);
            break;
        }

        semafor_p(sem, SEMAFOR_TASMA);
        tasma->ciezarowka = getpid();
        semafor_v(sem, SEMAFOR_TASMA);

        snprintf(buf, sizeof(buf), "Ciezarowka %d zajela stanowisko.\n", id);
        log_write(buf);

        g_odjedz_niepelna = 0;
        int pelna = 0;

        while (!g_odjedz_niepelna && !g_zakoncz_prace && !g_zakoncz_przyjmowanie && !pelna) {

            MsgP4Dostawa msg_express;

            if (sprawdz_dostawa_express(kolejka, &msg_express)) {
                snprintf(buf, sizeof(buf), "Ciezarowka %d: P4 ma %d paczek EXPRESS!\n", id, msg_express.ile_paczek);
                log_write(buf);

                MsgPotwierdzenie msg_gotowa = {
                    .mtype = MSG_CIEZAROWKA_GOTOWA,
                    .pojemnosc_wolna = (int)((max_waga - waga) / 25.0)
                };
                if (!wyslij_msg(kolejka, &msg_gotowa, sizeof(msg_gotowa) - sizeof(long),"Ciezarowka: blad wysylania gotowosci\n"))
                    break;

                int wynik = odbierz_express(sem, okienko, kolejka, &waga, &pojemnosc, &liczba_paczek, &ekspres, max_waga, max_pojemnosc, id, buf);
                if (wynik == -1) {
                    pelna = 1;
                    break;
                }
                continue;
            }

            if (sprawdz_dostawa_express(kolejka, &msg_express)) {
                MsgPotwierdzenie msg_gotowa = {
                    .mtype = MSG_CIEZAROWKA_GOTOWA,
                    .pojemnosc_wolna = (int)((max_waga - waga) / 25.0)
                };
                if (!wyslij_msg(kolejka, &msg_gotowa, sizeof(msg_gotowa) - sizeof(long), "Ciezarowka: blad wysylania gotowosci\n"))
                    break;

                int wynik = odbierz_express(sem, okienko, kolejka, &waga, &pojemnosc, &liczba_paczek, &ekspres, max_waga, max_pojemnosc, id, buf);
                if (wynik == -1) {
                    pelna = 1;
                }
                continue;
            }

            if (!semafor_p(sem, SEMAFOR_PACZKI)) {
                if (g_zakoncz_prace || g_zakoncz_przyjmowanie || g_odjedz_niepelna)
                    break;
                continue;
            }

            if (g_odjedz_niepelna || g_zakoncz_prace || g_zakoncz_przyjmowanie) {
                semafor_v(sem, SEMAFOR_PACZKI);
                break;
            }

            if (sprawdz_dostawa_express(kolejka, &msg_express)) {
                semafor_v(sem, SEMAFOR_PACZKI);

                MsgPotwierdzenie msg_gotowa = {
                    .mtype = MSG_CIEZAROWKA_GOTOWA,
                    .pojemnosc_wolna = (int)((max_waga - waga) / 25.0)
                };
                if (!wyslij_msg(kolejka, &msg_gotowa, sizeof(msg_gotowa) - sizeof(long), "Ciezarowka: blad wysylania gotowosci\n"))
                    break;

                int wynik = odbierz_express(sem, okienko, kolejka, &waga, &pojemnosc, &liczba_paczek, &ekspres, max_waga, max_pojemnosc, id, buf);
                if (wynik == -1) {
                    pelna = 1;
                    break;
                }
                continue;
            }

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

                snprintf(buf, sizeof(buf), "Ciezarowka %d: +Paczka ID %d (%.3f kg) [%.3f/%d kg]\n", id, paczka.id, paczka.waga, waga, max_waga);
                log_write(buf);

                semafor_v(sem, SEMAFOR_TASMA);
                semafor_v(sem, SEMAFOR_WOLNE_MIEJSCA);
                semafor_v(sem, SEMAFOR_WAGA_DOSTEPNA);
            } else {
                semafor_v(sem, SEMAFOR_TASMA);
                semafor_v(sem, SEMAFOR_PACZKI);

                if (waga + paczka.waga > max_waga) {
                    snprintf(buf, sizeof(buf), "Ciezarowka %d: PELNA - przekroczenie wagi (%.3f + %.3f > %d kg)\n", id, waga, paczka.waga, max_waga);
                } else {
                    snprintf(buf, sizeof(buf), "Ciezarowka %d: PELNA - przekroczenie objetosci (%.4f + %.4f > %d m3)\n",id, pojemnosc, paczka.objetosc, max_pojemnosc);
                }
                log_write(buf);

                snprintf(buf, sizeof(buf), "Ciezarowka %d: Stan zaladunku: %.3f/%d kg, %.4f/%d m3, %d paczek\n",id, waga, max_waga, pojemnosc, max_pojemnosc, liczba_paczek);
                log_write(buf);

                pelna = 1;
                break;
            }
        }

        semafor_p(sem, SEMAFOR_TASMA);
        tasma->ciezarowka = 0;
        semafor_v(sem, SEMAFOR_TASMA);
        semafor_v(sem, SEMAFOR_CIEZAROWKI);

        if (liczba_paczek > 0) {
            if (g_odjedz_niepelna) {
                snprintf(buf, sizeof(buf), "Ciezarowka %d: ODJAZD (wymuszony SIGUSR1) - niepelna\n", id);
            } else if (pelna) {
                snprintf(buf, sizeof(buf), "Ciezarowka %d: ODJAZD - pelna (brak miejsca na kolejna paczke)\n", id);
            } else {
                snprintf(buf, sizeof(buf), "Ciezarowka %d: ODJAZD - koniec przyjmowania\n", id);
            }
            log_write(buf);

            snprintf(buf, sizeof(buf), "Ciezarowka %d: Ladunek: %d paczek (%d EXPRESS), %.3f/%d kg, %.4f/%d m3\n", id, liczba_paczek, ekspres, waga, max_waga, pojemnosc, max_pojemnosc);
            log_write(buf);

            // Zakomentowac (jesli chce sie uruchomic bez sleepow)
            time_t czas = czas_rozwozu;
            while (czas > 0) {
                czas = sleep(czas);
            }
            // Koniec komentarza (do uruchomienia bez sleepow)

            snprintf(buf, sizeof(buf), "Ciezarowka %d: Zakonczyla rozwoz paczek\n", id);
            log_write(buf);

            snprintf(buf, sizeof(buf), "Ciezarowka %d: Wraca do magazynu\n", id);
            log_write(buf);

            waga = pojemnosc = 0;
            liczba_paczek = ekspres = 0;
        }

        if (g_zakoncz_przyjmowanie) {
            semafor_p(sem, SEMAFOR_TASMA);
            int zostalo = tasma->aktualna_ilosc;
            semafor_v(sem, SEMAFOR_TASMA);
            if (zostalo == 0)
                break;
        }
    }

    if (liczba_paczek > 0) {
        snprintf(buf, sizeof(buf), "Ciezarowka %d: OSTATNI KURS [%d paczek, %.3fkg]\n", id, liczba_paczek, waga);
        log_write(buf);
    }

    shmdt(tasma);
    shmdt(okienko);

    snprintf(buf, sizeof(buf), "Ciezarowka %d zakonczyla prace.\n", id);
    log_write(buf);

    sem_log_close();
    log_close();

    return 0;
}
