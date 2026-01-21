#include "utils.h"

static int g_paczek_na_ture = 0;
static int g_interwal = 10;
static volatile sig_atomic_t g_alarm_fired = 0;
static int g_wygenerowano = 0;
static int g_limit = 0;

static void handler_sigalrm(int sig) {
  (void)sig;
  g_alarm_fired = 1;
}

int main(int argc, char *argv[]) {
  srand(time(NULL) ^ getpid());

  if (argc < 10) {
    fprintf(stderr,
            "Uzycie: %s id sem shm_tasma shm_okienko log_dir kolejka paczki "
            "interwal shm_licznik\n",
            argv[0]);
    return 1;
  }

  int id = atoi(argv[1]);
  int sem = atoi(argv[2]);
  int shm_tasma = atoi(argv[3]);
  strncpy(g_log_dir, argv[5], sizeof(g_log_dir) - 1);
  g_paczek_na_ture = atoi(argv[7]);
  g_interwal = atoi(argv[8]);
  int shm_licznik = atoi(argv[9]);

  g_limit = g_paczek_na_ture / 3;
  if (g_limit < 1)
    g_limit = 10;

  if (id < 1 || id > 3) {
    fprintf(stderr, "Ten program obsluguje tylko pracownikow P1-P3\n");
    return 1;
  }

  ustaw_handlery_pracownik(id);
  signal(SIGALRM, handler_sigalrm);

  log_init(sem, "pracownicy.log", COL_CYAN);
  sem_log_init();

  Tasma *tasma = (Tasma *)shmat(shm_tasma, NULL, 0);
  if (tasma == (Tasma *)(-1)) {
    log_error("Blad dostepu do pamieci dzielonej tasmy\n");
    return 1;
  }

  LicznikId *licznik = (LicznikId *)shmat(shm_licznik, NULL, 0);
  if (licznik == (LicznikId *)(-1)) {
    log_error("Blad dostepu do pamieci dzielonej licznika\n");
    shmdt(tasma);
    return 1;
  }

  char buf[256];
  snprintf(buf, sizeof(buf), "P%d (PID %d) uruchomiony\n", id, getpid());
  log_write(buf);

  alarm(g_interwal);

  while (!g_zakoncz_prace) {
    pause();

    if (!licznik->generowanie_aktywne) {
      snprintf(buf, sizeof(buf), "P%d: Generowanie wylaczone - koncze\n", id);
      log_write(buf);
      break;
    }

    if (!g_alarm_fired) {
      alarm(g_interwal);
      continue;
    }

    g_alarm_fired = 0;
    alarm(g_interwal);

    int do_gen = 10;

    for (int i = 0; i < do_gen && !g_zakoncz_prace; i++) {
      int paczka_id = pobierz_nastepne_id(sem, licznik);
      Paczka p = generuj_paczke_zwykla(paczka_id);

      snprintf(buf, sizeof(buf),"P%d: Wygenerowano paczke ID=%d typ=%s waga=%.3fkg\n", id, p.id, nazwa_typu(p.typ), p.waga);
      log_write(buf);

      int paczka_polozono = 0;

      while (!paczka_polozono && !g_zakoncz_prace) {
        if (!semafor_p(sem, SEMAFOR_WOLNE_MIEJSCA)) {
          break;
        }

        if (g_zakoncz_prace) {
          semafor_v(sem, SEMAFOR_WOLNE_MIEJSCA);
          break;
        }

        if (!semafor_p(sem, SEMAFOR_TASMA)) {
          semafor_v(sem, SEMAFOR_WOLNE_MIEJSCA);
          break;
        }

        if (tasma->aktualna_waga + p.waga > tasma->max_waga) {
          semafor_v(sem, SEMAFOR_TASMA);
          semafor_v(sem, SEMAFOR_WOLNE_MIEJSCA);

          snprintf(buf, sizeof(buf),"P%d: Paczka ID=%d za ciezka (%.3f + %.3f > %d), czekam...\n", id,p.id, tasma->aktualna_waga, p.waga, tasma->max_waga);
          log_write(buf);

          //usleep(100000); 
          continue;
        }

        tasma->bufor[tasma->head] = p;
        tasma->head = (tasma->head + 1) % tasma->max_pojemnosc;
        tasma->aktualna_ilosc++;
        tasma->aktualna_waga += p.waga;
        g_wygenerowano++;
        paczka_polozono = 1;

        snprintf(buf, sizeof(buf),"P%d: Polozyl ID=%d na tasmie (ilosc=%d/%d, waga=%.2f/%dkg)\n",id, p.id, tasma->aktualna_ilosc, tasma->max_pojemnosc,tasma->aktualna_waga, tasma->max_waga);
        log_write(buf);

        semafor_v(sem, SEMAFOR_TASMA);
        semafor_v(sem, SEMAFOR_PACZKI);
      }
    }
  }

  alarm(0);

  snprintf(buf, sizeof(buf), "P%d: Koncze prace (wygenerowano %d paczek)\n", id,g_wygenerowano);
  log_write(buf);

  shmdt(tasma);
  shmdt(licznik);
  sem_log_close();
  log_close();

  return 0;
}
