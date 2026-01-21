# Projekt SO - Symulacja magazynu firmy spedycyjnej - Temat: 10

## Srodowisko
- **System:** Ubuntu 22.04.5 LTS
- **Kernel:** Linux 6.6.87.2-microsoft-standard-WSL2
- **Architektura:** x86_64
- **Kompilator:** gcc (Ubuntu 11.4.0-1ubuntu1~22.04.2) 11.4.0

### Wymagania wstępne

* `tmux`
* `make`

### Kompilacja
```
make clean && make
```

### Aby rozpocząć symulację, w katalogu projektu należy wykonać polecenie:
```bash
chmod 711 run.sh
./run.sh
```
Skrypt run.sh automatycznie kompiluje projekt (przy użyciu Makefile) i uruchamia symulację.


## Wprowadzenie
Celem projektu jest wieloprocesowa symulacja pracy magazynu firmy spedycyjnej.
System sklada sie z procesu nadrzednego oraz procesow pracownikow i ciezarowek.
Komunikacja i synchronizacja odbywa sie z uzyciem mechanizmow IPC Systemu V.

## Opis procesu i roli
- **`magazyn`**: inicjalizuje zasoby IPC, uruchamia procesy potomne i sprzata zasoby po zakonczeniu.
- **`dyspozytor`**: wysyla sygnaly sterujace do procesu ciezarowki, pracownika P4 oraz sygnał zakończenia do pracowników P1-P3 (SIGUSR1/SIGUSR2/SIGTERM).
- **`pracownicy` (P1-P3)**: generuja paczki zwykle i umieszczaja je na tasmie.
- **`pracownik4` (P4)**: generuje paczki EXPRESS i przekazuje je do ciezarowki poza tasma.
- **`ciezarowki`**: odbieraja paczki z tasmy oraz z "okienka" EXPRESS, ladujac je do pojazdow.

## Mechanizmy IPC i synchronizacja
- **Pamiec dzielona**: wspolny stan tasmy (pamięć dzielona - typu RING), "okienka" EXPRESS, licznik numeru ID paczek.
- **Semafory**: ochrona sekcji krytycznych (tasma, licznik ID, stanowisko ciezarowki, okienko EXPRESS).
- **Kolejki komunikatow**: wymiana sygnalow pomocniczych miedzy P4 a ciezarowkami (gotowosc/odbior).
- **Sygnaly**: sterowanie cyklem pracy (SIGUSR1 - odjazd niepelnej, SIGUSR2 - dostawa EXPRESS, SIGTERM - zakonczenie).

## Pseudokody (wybrane mechanizmy)

### 1) Odkładanie paczki na tasme (P1-P3)
```
PETLA co interwal:
  generuj paczke
  P(SEM_WOLNE_MIEJSCA)
  P(SEM_TASMA)
  jesli tasma_waga + paczka_waga > max_waga:
    V(SEM_TASMA)
    V(SEM_WOLNE_MIEJSCA)
    kontynuuj
  wstaw paczke na tasme ring
  zwieksz licznik paczek i wage
  V(SEM_TASMA)
  V(SEM_PACZKI)
```

### 2) Odbior paczek z tasmy (ciezarowka)
```
P(SEM_CIEZAROWKI)
ustaw PID ciezarowki przy tasmie
PETLA do pelnej:
  P(SEM_PACZKI)
  P(SEM_TASMA)
  zdejmij paczke z bufora ring
  zaktualizuj stan tasmy
  V(SEM_TASMA)
  V(SEM_WOLNE_MIEJSCA)
  zaladuj do ciezarowki
zwolnij stanowisko
V(SEM_CIEZAROWKI)
```

### 3) Dostawa paczek EXPRESS (P4 -> ciezarowka)
```
CZEKANIE na SIGUSR2
generuj pakiet EXPRESS
wyslij info do ciezarowki (kolejka komunikatow)
czekaj na gotowosc ciezarowki
PETLA az odbior calego pakietu:
  P(SEM_EXPRESS)
  wstaw paczki do okienka
  V(SEM_EXPRESS)
  wyslij "paczki przekazane"
  czekaj na potwierdzenie odebrania
```

## Elementy wyroziajace
- **Synchronizacja logow**: wpisy nie nakladaja sie dzieki blokowaniu sekcji wyjscia.
- **Kolorowanie logow**: dzieki zastosowaniu kolorowania logow podczas dzialania programu, zwieksza sie ich czytelnosc. 
- **Sterowanie sygnalami w panelu tmux**: zewnetrzne sterowanie odjazdem dostawa EXPRESS oraz zakonczenie dzialania programu dzieki uzyciu tmux.

## Problemy i rozwiazania
- **Opóznione logi**: Program posiadał problemy z regularnym wypisywaniem logów. Udało się to rozwiązać dzieki ustawieniu buforowania na _IONBF i wymuszenia fflush().
- **Zatrzymania przy duzym obciazeniu**: wzmocnione odblokowanie procesow podczas zakonczenia.
- **Zakleszczenia przy Express**: Po dodaniu kolejki komunikatów miedzy pracownikiem P4 a ciezarowka przy tasmie dochodzilo do licznych zakleszczeń, rozwiązano to dzieki poprawie kolejnosci semaforów.

## Testy
Poniższe testy zawieraja gotowe parametry startowe.

### Test 1 - Pojemnosc tasmy i ograniczenie wagi
- **Cel:** sprawdzenie, ze tasma nie przekracza limitu K oraz M.
- **Parametry:** liczba_ciezarowek (N)=1000, ladownosc_kg (W)=25, pojemnosc_m3 (V)=1, max_paczek_na_tasmie (K)=2, max_waga_tasmy (M)=25, paczki_na_ture=1000, interwal=10, paczek_express=1000
- **Oczekiwany rezultat:** pracownicy nie daja paczki na tasme
- **LOGI:** 
```
[14:41:51.958] P3: Wygenerowano paczke ID=6 typ=C (Duza) waga=20.744kg
[14:41:51.969] P2: Zwolniono wage na tasmie - ponawiam probe
[14:41:51.971] P3: Polozyl ID=6 na tasmie (ilosc=2/2, waga=20.98/25kg)
[14:41:51.981] P3: Wygenerowano paczke ID=7 typ=C (Duza) waga=17.270kg
[14:41:51.981] P1: Zwolniono wage na tasmie - ponawiam probe
[14:41:51.986] P2: Brak wagi na tasmie dla ID=4 (20.744 + 18.363 > 25) - czekam na zwolnienie wagi
[14:41:51.993] P3: Brak wagi na tasmie dla ID=7 (20.744 + 17.270 > 25) - czekam na zwolnienie wagi
[14:41:51.998] P1: Brak wagi na tasmie dla ID=2 (20.744 + 15.683 > 25) - czekam na zwolnienie wagi
[14:41:52.011] P2: Zwolniono wage na tasmie - ponawiam probe
[14:41:52.016] P2: Polozyl ID=4 na tasmie (ilosc=1/2, waga=18.36/25kg)
```
- **Wynik:** [Zaliczono]

### Test 2 - Zaladunek express na zadanie
- **Cel:** obsluga SIGUSR2 i dostawa paczek EXPRESS do ciezarowki.
- **Parametry:** liczba_ciezarowek (N)=1000, ladownosc_kg (W)=25, pojemnosc_m3 (V)=1, max_paczek_na_tasmie (K)=2, max_waga_tasmy (M)=25, paczki_na_ture=1000, interwal=10, paczek_express=1000
- **Kroki:** podczas dzialania wyslac SIGUSR2 do P4.
- **Oczekiwany rezultat:** P4 generuje paczki EXPRESS i ciezarowka je odbiera przed zwykłymi paczkami.
- **LOGI:** 
```
[14:42:33.959] DYSPOZYTOR: SIGUSR2 -> P4 PID 36662

[14:42:33.961] P4: Generuje 1000 paczek EXPRESS...
[14:42:33.966] P4: Paczka EXPRESS ID=151 typ=C (Duza) waga=18.573kg
[14:42:33.970] P4: Paczka EXPRESS ID=152 typ=A (Mala) waga=1.046kg
[14:42:33.973] P4: Paczka EXPRESS ID=153 typ=C (Duza) waga=19.586kg
[14:42:33.977] P4: Paczka EXPRESS ID=154 typ=C (Duza) waga=24.264kg
...
[14:42:32.637] Ciezarowka 70: +Paczka ID 150 (14.192 kg) [14.192/25 kg]
[14:42:41.961] Ciezarowka 70: Odbieram paczki EXPRESS (priorytet!)...
[14:42:41.966] Ciezarowka 70: +EXPRESS ID 1150 (3.060 kg) [17.252/25 kg, 0.0657/1 m3]
[14:42:41.968] Ciezarowka 70: Odebrano 1 EXPRESS, zostalo: 999
[14:42:41.972] Ciezarowka 70: ODJAZD - pelna (brak miejsca na kolejna paczke)
[14:42:41.974] Ciezarowka 70: Ladunek: 2 paczek (1 EXPRESS), 17.252/25 kg, 0.0657/1 m3
```
- **Wynik:** [Zaliczono]

### Test 3 - Odjazd niepelnej ciezarowki
- **Cel:** obsluga SIGUSR1 dla ciezarowki stojacej przy tasmie.
- **Parametry:** liczba_ciezarowek (N)=100, ladownosc_kg (W)=1000, pojemnosc_m3 (V)=5, max_paczek_na_tasmie (K)=100, max_waga_tasmy (M)=2000, paczki_na_ture=1000, interwal=2, paczek_express=1000
- **Kroki:** podczas postoju ciezarowki wyslac SIGUSR1.
- **Oczekiwany rezultat:** ciezarowka odjezdza bez pelnego zaladunku, pojawia sie kolejna.
- **LOGI:**
```
[14:53:02.375] DYSPOZYTOR: SIGUSR1 -> ciezarowka PID 37887

[14:53:03.834] Ciezarowka 22: +Paczka ID 89 (8.970 kg) [347.684/1000 kg]
[14:53:03.840] Ciezarowka 22: +Paczka ID 90 (5.464 kg) [353.148/1000 kg]
[14:53:04.322] Ciezarowka 22: ODJAZD (wymuszony SIGUSR1) - niepelna
[14:53:04.323] Ciezarowka 22: Ladunek: 30 paczek (0 EXPRESS), 353.148/1000 kg, 1.8678/5 m3
[14:53:04.325] Ciezarowka 4 zajela stanowisko.
[14:53:05.048] Ciezarowka 4: +Paczka ID 91 (0.175 kg) [0.175/1000 kg]
[14:53:05.064] Ciezarowka 4: +Paczka ID 92 (15.821 kg) [15.996/1000 kg]
```
- **Wynik:** [Zaliczono]

### Test 4 - Zakonczenie symulacji
- **Cel:** poprawne sprzatanie zasobow po SIGTERM.
- **Parametry:** liczba_ciezarowek (N)=100, ladownosc_kg (W)=1000, pojemnosc_m3 (V)=5, max_paczek_na_tasmie (K)=100, max_waga_tasmy (M)=2000, paczki_na_ture=1000, interwal=2, paczek_express=1000, czas_rozwozu 45 s
- **Kroki:** wyslac sygnal 3
- **Oczekiwany rezultat:** ciezarowki rozwoza paczki i koncza, pracownicy koncza prace, IPC zostaje usuniete.
- **LOGI:**
```
[14:58:40.655] DYSPOZYTOR: Wysylam SIGTERM do magazynu

[14:58:40.658] P2: Koncze prace 
[14:58:40.658] P1: Koncze prace 
[14:58:40.658] P3: Koncze prace

[14:58:40.658] P4: Koncze prace

[14:58:40.676] Ciezarowka 9: ODJAZD - koniec przyjmowania
[14:58:40.678] Ciezarowka 9: Ladunek: 90 paczek (0 EXPRESS), 931.831/1000 kg, 4.7740/5 m3
[14:59:25.165] Ciezarowka 9: Zakonczyla rozwoz paczek
[14:59:25.168] Ciezarowka 9: Wraca do magazynu
[14:59:25.173] Ciezarowka 9 zakonczyla prace.

[14:58:40.841] Czekam az ciezarowki rozwioza przesylki i wroca...
[14:59:25.174] Wszystkie ciezarowki wrocily do magazynu.
[14:59:25.176] Wszyscy pracownicy i ciezarowki zakonczone.
[14:59:25.177] === SYMULACJA ZAKONCZONA ===
[14:59:25.178] CLEANUP: Sprzatanie zasobow...
[14:59:25.179] CLEANUP: Zakonczone
```
- **Wynik:** [Zaliczono]

### Test 5 - Praca bez sleep/usleep
- **Cel:** potwierdzenie, ze symulacja nie uzywa sleepa jako "mechanizmu synchroizacji"
- **Parametry:** liczba_ciezarowek (N)=1000, ladownosc_kg (W)=1000, pojemnosc_m3 (V)=5, max_paczek_na_tasmie (K)=250, max_waga_tasmy (M)=4000, paczki_na_ture=1000, paczek_express=1000
- **Kroki:** zakomentowac wszystkie `sleep` i `usleep`, uruchomic symulacje.
- **Oczekiwany rezultat:** symulacja dziala poprawnie mimo 100% CPU.
- **LOGI:**
```
[15:08:33.517] Ciezarowka 8: +Paczka ID 89 (0.531 kg) [977.455/1000 kg]
[15:08:33.525] Ciezarowka 8: PELNA - przekroczenie objetosci (4.9151 + 0.0997 > 5 m3)
[15:08:33.526] Ciezarowka 8: Stan zaladunku: 977.455/1000 kg, 4.9151/5 m3, 89 paczek
[15:08:33.530] Ciezarowka 8: ODJAZD - pelna (brak miejsca na kolejna paczke)
[15:08:33.531] Ciezarowka 8: Ladunek: 89 paczek (0 EXPRESS), 977.455/1000 kg, 4.9151/5 m3
[15:08:33.532] Ciezarowka 8: Zakonczyla rozwoz paczek
[15:08:33.534] Ciezarowka 8: Wraca do magazynu

[15:09:53.354] Otrzymano sygnal 3 - Zamykanie symulacji...
[15:09:55.084] Czekam na zakonczenie pracownikow...
[15:09:55.085] Wszyscy pracownicy zakonczone.
[15:09:55.087] Czekam az ciezarowki rozwioza przesylki i wroca...
[15:09:55.088] Wszystkie ciezarowki wrocily do magazynu.
[15:09:55.089] Wszyscy pracownicy i ciezarowki zakonczone.
[15:09:55.090] === SYMULACJA ZAKONCZONA ===
[15:09:55.091] CLEANUP: Sprzatanie zasobow...
[15:09:55.092] CLEANUP: Zakonczone

```
- **Wynik:** [Zaliczono]

## Linki do kodu (permalinki)
Poniższe linki prowadzą do konkretnych fragmentów kodu w repozytorium GitHub (stałe linki do commit-a), zgodnie z wymaganiami 5.2.

a. **Tworzenie i obsługa plików (creat(), open(), close(), read(), write(), unlink())**
- `open()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/utils.c#L151-L155
- `close()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/utils.c#L159-L162
- `write()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/utils.c#L176-L180

b. **Tworzenie procesów (fork(), exec(), exit(), wait())**
- `fork()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/magazyn.c#L79
- `exec()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/magazyn.c#L86
- `exit()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/magazyn.c#L88

c. **Obsługa sygnałów (kill(), raise(), signal(), sigaction())**
- `kill()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/dyspozytor.c#L39-L41
- `signal()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/magazyn.c#L104-L105

d. **Synchronizacja procesów (ftok(), semget(), semctl(), semop())**
- `ftok()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/utils.c#L469
- `semget()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/utils.c#L286-L288
- `semctl()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/utils.c#L295-L304
- `semop()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/utils.c#L316-L334

e. **Segmenty pamięci dzielonej (ftok(), shmget(), shmat(), shmdt(), shmctl())**
- `shmget()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/magazyn.c#L151
- `shmat()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/magazyn.c#L152-L155
- `shmdt()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/magazyn.c#L23-L25
- `shmctl()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/magazyn.c#L26-L28

f. **Kolejki komunikatów (ftok(), msgget(), msgsnd(), msgrcv(), msgctl())**
- `ftok()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/utils.c#L469
- `msgget()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/utils.c#L474
- `msgsnd()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/ciezarowki.c#L3-L4
- `msgrcv()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/ciezarowki.c#L30-L31
- `msgctl()` — https://github.com/LMrosz/SO_Magazyn_firmy_spedycyjnej/blob/a39612a89577c5096e968ce77c25e21304d9773a/utils.c#L481-L483

