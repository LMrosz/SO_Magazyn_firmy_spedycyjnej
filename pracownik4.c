#include "utils.h"

static int g_paczek_w_pakiecie = 100;

int main(int argc, char *argv[]) {
  srand(time(NULL) ^ getpid());

  if (argc < 10) {fprintf(stderr,"Uzycie: %s id sem shm_tasma shm_okienko log_dir kolejka paczki interwal shm_licznik\n",argv[0]);
    return 1;
  }

  int sem = atoi(argv[2]);
  int shm_okienko = atoi(argv[4]);
  strncpy(g_log_dir, argv[5], sizeof(g_log_dir) - 1);
  int kolejka = atoi(argv[6]);
  g_paczek_w_pakiecie = atoi(argv[7]);
  int shm_licznik = atoi(argv[9]);

  ustaw_handlery_pracownik(4);

  log_init(sem, "pracownik4.log", COL_MAGENTA);
  sem_log_init();

  OkienkoEkspresShm *okienko = (OkienkoEkspresShm *)shmat(shm_okienko, NULL, 0);
  if (okienko == (OkienkoEkspresShm *)(-1)) {
    log_error("P4: Blad dostepu do pamieci dzielonej okienka\n");
    return 1;
  }

  LicznikId *licznik = (LicznikId *)shmat(shm_licznik, NULL, 0);
  if (licznik == (LicznikId *)(-1)) {
    log_error("P4: Blad dostepu do pamieci dzielonej licznika\n");
    shmdt(okienko);
    return 1;
  }

  char buf[256];
  snprintf(buf, sizeof(buf),"P4 (PID %d) - dostawa paczek EXPRESS (pakiet=%d paczek)\n",getpid(), g_paczek_w_pakiecie);
  log_write(buf);
  log_write("P4: Czekam na sygnal SIGUSR2 od dyspozytora...\n");

  while (!g_zakoncz_prace) {
    pause();

    if (g_zakoncz_prace)
      break;

    if (!g_dostarcz_ekspres)
      continue;

    g_dostarcz_ekspres = 0;

    log_write("P4: Otrzymalem SIGUSR2 - rozpoczynam dostawe paczek EXPRESS!\n");

    int ile_paczek = g_paczek_w_pakiecie;
    if (ile_paczek < 1)
      ile_paczek = 1;

    Paczka *paczki = malloc(ile_paczek * sizeof(Paczka));
    if (!paczki) {
      log_error("P4: Blad alokacji pamieci!\n");
      continue;
    }

    snprintf(buf, sizeof(buf), "P4: Generuje %d paczek EXPRESS...\n",ile_paczek);
    log_write(buf);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand((unsigned int)(tv.tv_sec ^ tv.tv_usec ^ getpid()));

    for (int i = 0; i < ile_paczek && !g_zakoncz_prace; i++) {
      int id = pobierz_nastepne_id(sem, licznik);
      paczki[i] = generuj_paczke_ekspres(id);

      snprintf(buf, sizeof(buf),"P4: Paczka EXPRESS ID=%d typ=%s waga=%.3fkg\n", paczki[i].id,nazwa_typu(paczki[i].typ), paczki[i].waga);
      log_write(buf);
    }

    if (g_zakoncz_prace) {
      free(paczki);
      break;
    }

    MsgP4Dostawa msg_dostawa = {.mtype = MSG_P4_DOSTAWA_GOTOWA,.ile_paczek = ile_paczek,.nadawca_pid = getpid()};

    snprintf(buf, sizeof(buf), "P4: Wysylam komunikat o dostawie (%d paczek)...\n", ile_paczek);
    log_write(buf);

    if (msgsnd(kolejka, &msg_dostawa, sizeof(msg_dostawa) - sizeof(long), 0) ==
        -1) {
      if (errno == EIDRM || errno == EINVAL) {
        log_write("P4: Kolejka usunieta - koncze\n");
        free(paczki);
        break;
      }
      log_error("P4: Blad wysylania komunikatu!\n");
      free(paczki);
      continue;
    }

    log_write("P4: Czekam na gotowosc ciezarowki...\n");

    MsgPotwierdzenie msg_gotowa;
    int got_response = 0;

    while (!g_zakoncz_prace && !got_response) {
      ssize_t ret =
          msgrcv(kolejka, &msg_gotowa, sizeof(msg_gotowa) - sizeof(long),
                 MSG_CIEZAROWKA_GOTOWA, 0);
      if (ret != -1) {
        got_response = 1;
        break;
      }
      if (errno == EIDRM || errno == EINVAL) {
        g_zakoncz_prace = 1;
        break;
      }
      if (errno == EINTR)
        continue;
      log_error("P4: Blad odbierania potwierdzenia!\n");
      break;
    }

    if (g_zakoncz_prace || !got_response) {
      free(paczki);
      continue;
    }

    snprintf(buf, sizeof(buf), "P4: Ciezarowka gotowa!\n");
    log_write(buf);

    int ile_pozostalo = ile_paczek;
    int offset = 0;

    while (ile_pozostalo > 0 && !g_zakoncz_prace) {
      if (!semafor_p_p4(sem, SEMAFOR_EXPRESS)) {
        if (g_zakoncz_prace)
          break;
        continue;
      }

      okienko->ilosc = 0;

      int do_skopiowania =
          (ile_pozostalo > MAX_PACZEK) ? MAX_PACZEK : ile_pozostalo;
      for (int i = 0; i < do_skopiowania; i++) {
        okienko->paczki[okienko->ilosc++] = paczki[offset + i];
      }
      okienko->gotowe = 1;

      semafor_v(sem, SEMAFOR_EXPRESS);

      MsgP4Dostawa msg_przekazane = {.mtype = MSG_P4_PACZKI_PRZEKAZANE,.ile_paczek = do_skopiowania,.nadawca_pid = getpid()};

      if (msgsnd(kolejka, &msg_przekazane,
                 sizeof(msg_przekazane) - sizeof(long), 0) == -1) {
        log_error("P4: Blad wysylania komunikatu o przekazaniu!\n");
        break;
      }

      snprintf(buf, sizeof(buf), "P4: Wystawilem %d paczek, czekam na odbior...\n",do_skopiowania);
      log_write(buf);

      MsgPotwierdzenie msg_odebrano;
      int got_confirm = 0;

      while (!g_zakoncz_prace && !got_confirm) {
        ssize_t ret =
            msgrcv(kolejka, &msg_odebrano, sizeof(msg_odebrano) - sizeof(long), MSG_ODEBRANO_POTWIERDZENIE, 0);
        if (ret != -1) {
          got_confirm = 1;
          break;
        }
        if (errno == EIDRM || errno == EINVAL) {
          g_zakoncz_prace = 1;
          break;
        }
        if (errno == EINTR)
          continue;
        break;
      }

      if (g_zakoncz_prace)
        break;

      int odebrane = msg_odebrano.ile_odebranych;
      int nieodebrane = msg_odebrano.ile_zostalo;

      snprintf(buf, sizeof(buf), "P4: Ciezarowka odebrala %d paczek, zostalo: %d\n", odebrane, nieodebrane);
      log_write(buf);

      offset += odebrane;
      ile_pozostalo -= odebrane;

      if (nieodebrane > 0 && ile_pozostalo > 0) {
        log_write("P4: Ciezarowka pelna - czekam na nastepna...\n");

        msg_dostawa.ile_paczek = ile_pozostalo;

        if (msgsnd(kolejka, &msg_dostawa, sizeof(msg_dostawa) - sizeof(long),
                   0) == -1) {
          break;
        }

        int got_next = 0;

        while (!g_zakoncz_prace && !got_next) {
          ssize_t ret =
              msgrcv(kolejka, &msg_gotowa, sizeof(msg_gotowa) - sizeof(long), MSG_CIEZAROWKA_GOTOWA, 0);
          if (ret != -1) {
            got_next = 1;
            break;
          }
          if (errno == EIDRM || errno == EINVAL) {
            g_zakoncz_prace = 1;
            break;
          }
          if (errno == EINTR)
            continue;
          break;
        }
      }
    }

    free(paczki);

    if (ile_pozostalo > 0) {
      snprintf(buf, sizeof(buf),"P4: Dostawa przerwana - nie dostarczono %d paczek\n",ile_pozostalo);
      log_write(buf);
    } else {
      log_write("P4: Dostawa zakonczona pomyslnie!\n");
    }
  }

  log_write("P4: Koncze prace\n");

  shmdt(okienko);
  shmdt(licznik);
  sem_log_close();
  log_close();

  return 0;
}
