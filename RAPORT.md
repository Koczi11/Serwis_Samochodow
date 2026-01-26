# SERWIS SAMOCHODÓW - RAPORT (temat: 6)

### Autor: Kacper Koczera
#### Nr albumu: 155191

Wymagania projektu i szczegółowy opis są dostępne w pliku: [README.md](README.md)

---

## Pliki i ich działanie w projekcie

Projekt realizowano w języku C, wykorzystując механизмы komunikacji międzyprocesowej (IPC) System V: pamięć dzieloną (shared memory), kolejki komunikatów (message queues) i semafory (semaphores).

Program można pobrać i zbudować w następujących krokach:

1. `git clone https://github.com/Koczi11/Serwis_Samochodow.git`
2. `cd Serwis_Samochodow`
3. `make clean && make`
4. `./serwis`

### Opis poszczególnych procesów

* **main.c** - proces kierownika serwisu, odpowiedzialny za tworzenie wszystkich procesów (kasjer, pracownicy serwisu, mechanicy) oraz struktur komunikacyjnych (pamięć dzielona, semafory, kolejka komunikatów). Zarządza czasem symulacji (24-godzinny cykl) oraz kontroluje otwieranie/zamykanie serwisu. Obsługuje sygnały ewakuacyjne.

* **kierownik.c** - kierownik serwisu, proces będący rozszerzeniem głównego, który symuluje upływ czasu (1 sekunda rzeczywista = 1 godzina symulacyjna). Odpowiedzialny za otwieranie/zamykanie serwisu w określonych godzinach (8:00-18:00). Losowo wysyła sygnały do mechaników (przyspieszenie, powolnienie, zamknięcie stanowiska). Zarządza całym serwisem i obsługuje zdarzenia awaryjne (pożar - sygnał SIGUSR1).

* **pracownik_serwisu.c** - 3 procesy pracowników obsługujących kierowców. Odpowiadają za:
  - Sprawdzenie czy marka samochodu jest obsługiwana
  - Wycenę naprawy na podstawie wybranej usługi
  - Dynamiczne otwieranie/zamykanie okienek obsługi w zależności od liczby czekających kierowców (progi K1=3 i K2=5)
  - Zgłaszanie dodatkowych usterek (20% przypadków)
  - Komunikację z kierowcą o zaakceptowaniu warunków naprawy
  - Wysyłanie samochodu do mechanika (wyznaczenie wolnego stanowiska)
  - Przyjęcie informacji o ukończonej naprawie od mechanika
  - Komunikacja z kasjerem o opłacie

* **mechanik.c** - 8 procesów mechaników obsługujących stanowiska naprawy. Każdy mechanik:
  - Obsługuje samochód przez określony czas (może być przyspieszony o 50%)
  - Obsługuje sygnały kierownika (zamknięcie stanowiska, przyspieszenie, powolnienie)
  - Zgłasza pracownikowi serwisu o dodatkowych usterkach (20% prawdopodobieństwa)
  - Informuje pracownika o ukończeniu naprawy
  - Stanowiska 1-7 obsługują marki A, E, I, O, U, Y
  - Stanowisko 8 obsługuje tylko marki U, Y

* **kierowca.c** - losowo generowani kierowcy przyjeżdżający do serwisu. Każdy kierowca:
  - Losuje markę samochodu (A-Z) i wymaganą usługę
  - Weryfikuje czy marka jest obsługiwana
  - Czeka na otwarcie serwisu (jeśli usterka krytyczna lub czas oczekiwania < 1h)
  - Rejestruje się u pracownika serwisu
  - Akceptuje lub odrzuca wycenę (2% szans na rezygnację)
  - Rozważa zaakceptowanie dodatkowych usterek (20% szans na dodatkowe usterki, 20% na odrzucenie)
  - Czeka na naprawę
  - Płaci w kasie i odbiera samochód

* **kasjer.c** - jeden proces kasjera odpowiedzialny za:
  - Przyjmowanie płatności od kierowców
  - Prowadzenie dziennego utargu
  - Generowanie raportu z przychodów
  - Obsługę ewakuacji w przypadku pożaru

* **serwis_ipc.c** - moduł wspólny zawierający implementację:
  - Inicjalizacji i czyszczenia struktur IPC
  - Funkcji pomocniczych do obsługi semaforów (lock/unlock)
  - Funkcji do wysyłania i odbierania wiadomości
  - Funkcji sygnalizacyjnych (sem_wait, sem_signal)
  - Funkcji związanych z bezpiecznym czekaniem
  - Operacji na strukturach danych (usługi, samochody)
  - Logowania zdarzeń

* **serwis_ipc.h** - plik nagłówkowy zawierający:
  - Definicje stałych (porty, limity, identyfikatory procesów)
  - Struktury danych (Usluga, Samochod, Stanowisko, SharedData)
  - Deklaracje funkcji z serwis_ipc.c
  - Enumy typów komunikatów

---

## Z czym były problemy

### Synchronizacja dostępu do pamięci dzielonej
Głównym wyzwaniem było zapewnienie bezpiecznego dostępu do wspólnych struktur danych bez deadlock'ów. Rozwiązaniem było użycie semaforów binarnych z rygorystycznym zarządzaniem ich zwolnieniem.

### Obsługa sygnałów w kontekście wieloprocesowości
Problem polegał na tym, że sygnały wysyłane kierownikowi musiały być propagowane do odpowiednich procesów (mechaników). Rozwiązanie wymagało użycia `kill()` z SIGRTMIN/SIGRTMIN+1 itp. dla precyzyjnej kontroli.

### Zarządzanie stanem przyspieszenia stanowisk
Implementacja warunkowego przyspieszenia (tylko jedno przyspieszenie, możliwość powrotu do normy) wymagała dokładnego śledzenia stanu flagi `przyspieszony` w każdym mechaniku.

### Niespodziewane problemy z timeoutami
Pierwsze podejście do czekania oparte na `sleep()` było nieelastyczne. Zamiast tego wykorzystano semafory z krótkim czasem oczekiwania w pętli.

---

## Komunikacja między-procesowa

### 1. Pamięć dzielona (Shared Memory)
Struktura `SharedData` przechowuje globalny stan serwisu:
- **Stanowiska**: tablica stanowisk z informacją o zajętości i PID-em mechanika
- **serwis_otwarty**: flaga otwarcia/zamknięcia serwisu
- **aktualna_godzina**: bieżący czas symulacji (0-23)
- **pozar**: flaga ewakuacji pożaru
- **liczba_oczekujacych_klientow**: liczba kierowców w kolejce rejestracji
- **aktywne_okienka_obslugi**: liczba otwartych stanowisk obsługi
- **auta_w_serwisie**: liczba samochodów będących w serwisie

### 2. Kolejki komunikatów (System V Message Queues)
Jedyna kolejka komunikatów (`msg_id`) obsługuje wszystkie typy wiadomości:

- **MSG_REJESTRACJA (mtype=1)**: kierowca wysyła prośbę o rejestrację do pracownika serwisu
- **MSG_KIEROWCA(pid) (mtype=50000+pid)**: pracownik serwisu wysyła wycenę do kierowcy
- **MSG_DECYZJA_USLUGI(id) (mtype=10000+id*1000+1)**: kierowca odpowiada o akceptacji wyceny
- **MSG_DECYZJA_DODATKOWA(id) (mtype=10000+id*1000+2)**: kierowca decyduje o dodatkowych usterkach
- **MSG_OD_MECHANIKA (mtype=40)**: mechanik informuje pracownika o ukończeniu naprawy
- **MSG_POTWIERDZENIE_PLATNOSCI(id) (mtype=10000+id*1000+3)**: kasjer potwierdza opłatę
- **MSG_KASA (mtype=30)**: pracownik serwisu wysyła do kasjera informację o opłacie

### 3. Semafory (System V Semaphores)
Zbiór 5 semaforów kontroluje synchronizację:

- **SEM_SHARED (0)**: binarny semafor chroniący pamięć dzieloną (inicjalnie=1)
- **SEM_SERWIS_OTWARTY (1)**: sygnalizuje otwarcie serwisu (inicjalnie=0, kierowcy czekają tutaj)
- **SEM_NOWA_WIADOMOSC (2)**: sygnalizuje nową wiadomość w kolejce (inicjalnie=0)
- **SEM_WOLNY_MECHANIK (3)**: sygnalizuje dostępność wolnego mechanika (inicjalnie=0)
- **SEM_TIMER (4)**: pomocniczy semafor do implementacji timeoutów (inicjalnie=0)

---

## Model synchronizacji

Projekt wykorzystuje **model producent-konsument** z elementami sygnalizacji:

1. **Ochrona pamięci dzielonej**: Każdy dostęp do `shared` poprzedzony jest `sem_lock(SEM_SHARED)` i zakończony `sem_unlock(SEM_SHARED)`

2. **Synchronizacja wiadomości**: Procesy używające kolejki komunikatów sygnalizują się nawzajem za pośrednictwem `SEM_NOWA_WIADOMOSC` aby uniknąć busy-waiting

3. **Synchronizacja zdarzeń**: Kierowcy czekają na otwarcie serwisu (`SEM_SERWIS_OTWARTY`), mechanicy oczekują na nowe zlecenia

4. **Obsługa sygnałów**: 
   - Kierownik wysyła sygnały (SIGRTMIN, SIGRTMIN+1, SIGRTMIN+2) do mechaników
   - W przypadku pożaru wysyła SIGUSR1 do wszystkich procesów

---

## Sygnały

Każdy proces obsługuje sygnały w bezpieczny sposób:

- **SIGTERM, SIGINT**: przerwanie procesu (obsługiwane w main.c)
- **SIGCHLD**: czyszczenie procesów zombie (obsługiwane w main.c, handler `handle_sigchld`)
- **SIGUSR1**: sygnał pożaru - każdy proces musi obsługiwać ewakuację
  - Kierowcy: opuszczają serwis
  - Pracownicy serwisu: zamykają okienka i wychodzą
  - Mechanicy: przerywają pracę
  - Kasjer: zamyka kasę i wychodzi
- **SIGRTMIN, SIGRTMIN+1, SIGRTMIN+2**: sygnały kierownika dla mechaników
  - SIGRTMIN: zamknięcie stanowiska
  - SIGRTMIN+1: przyspieszenie pracy
  - SIGRTMIN+2: powrót do normalnej prędkości

---

## Linki do kluczowych fragmentów

### 1. Inicjalizacja struktur komunikacji
- [init_ipc() - tworzenie IPC](src/serwis_ipc.c#L25-L160)
- [cleanup_ipc() - czyszczenie IPC](src/serwis_ipc.c#L162-L191)

### 2. Operacje na semaforach
- [sem_lock() - operacja P (wait)](src/serwis_ipc.c#L230-L250)
- [sem_unlock() - operacja V (signal)](src/serwis_ipc.c#L252-L270)
- [signal_serwis_otwarty() - sygnalizacja otwarcia](src/serwis_ipc.c#L272-L285)
- [wait_serwis_otwarty() - oczekiwanie na otwarcie](src/serwis_ipc.c#L287-L300)

### 3. Operacje na kolejce komunikatów
- [send_msg() - wysłanie wiadomości](src/serwis_ipc.c#L380-L410)
- [recv_msg() - odbiór wiadomości](src/serwis_ipc.c#L412-L450)
- [signal_nowa_wiadomosc() - sygnalizacja nowej wiadomości](src/serwis_ipc.c#L452-L465)

### 4. Zarządzanie procesami
- [fork() - tworzenie procesów kierowców](src/main.c#L75-L100)
- [execl() - uruchamianie pracowników serwisu](src/main.c#L85-L95)
- [execl() - uruchamianie mechaników](src/main.c#L100-L115)
- [waitpid() - czyszczenie zombi](src/main.c#L12-L30)

### 5. Obsługa sygnałów
- [Obsługa SIGUSR1 w kierowcy](src/kierowca.c#L11-L17)
- [Obsługa sygnałów w mechaniku](src/mechanik.c#L11-L60)
- [Obsługa sygnałów w pracowniku serwisu](src/pracownik_serwisu.c#L70-L85)
- [Obsługa sygnałów w kierowniku](src/kierownik.c#L1-L20)

### 6. Dynamiczne zarządzanie okienkami
- [Sprawdzanie progów K1 i K2](src/pracownik_serwisu.c#L100-L150)
- [Otwieranie/zamykanie okienek](src/pracownik_serwisu.c#L150-L200)

### 7. Logowanie i raporty
- [zapisz_log() - zapis logu zdarzenia](src/serwis_ipc.c#L500-L530)
- [zapisz_raport() - zapis raportu dziennego](src/serwis_ipc.c#L532-L560)

---

## Wyróżniające elementy projektu

1. **Dynamiczne zarządzanie okienkami obsługi**: System automatycznie otwiera/zamyka stanowiska obsługi na podstawie liczby czekających kierowców (progi K1=3 i K2=5)

2. **Obsługa dodatkowych usterek**: 20% samochodów otrzymuje dodatkowe usterki podczas diagnostyki, a kierowcy mogą je zaakceptować lub odrzucić

3. **Inteligentne kierowanie do stanowisk**: Stanowiska 1-7 obsługują większość marek, natomiast stanowisko 8 dedykowane jest tylko markom U i Y

4. **Wielopoziomowa synchronizacja czasowa**: Symulacja czasu niezależna od rzeczywistych opóźnień, bezpieczna dla sygnałów

5. **Pełne logowanie zdarzeń**: Wszystkie zdarzenia (rejestracja, decyzje, naprawy, płatności) są zapisywane do logu

6. **Obsługa ewakuacji**: Sygnał pożaru (SIGUSR1) natychmiast przerywa wszystkie operacje i wymusza wyjście z serwisu

---

## Kluczowe pseudokody

### 1. main.c (Proces kierownika serwisu)

```pseudocode
GŁÓWNY():
    srand(time(NULL))
    init_ipc(1)  // Tworzenie struktur komunikacji
    
    // Uruchamianie procesów
    fork() -> execl("./kasjer")
    LOOP i=0 TO 2:
        fork() -> execl("./pracownik_serwisu", i)
    LOOP i=0 TO 7:
        fork() -> execl("./mechanik", i)
    
    // Uruchamianie kierownika (symulacja czasu)
    fork() -> kierownik_process()
    
    // Pętla główna
    WHILE running:
        // Losowe generowanie kierowców
        IF rand() % 100 < PRAWDOPODOBIENSTWO_KIEROWCY:
            fork() -> execl("./kierowca")
        
        wait(SIGCHLD)  // Zbieranie zombie
    
    // Zamykanie
    cleanup_ipc()
```

### 2. kierownik.c (Kierownik czasu)

```pseudocode
KIEROWNIK_CZASU():
    init_ipc(0)  // Dołączenie do IPC
    aktualna_godzina = 6
    
    WHILE TRUE:
        safe_wait_seconds(SEC_PER_H)  // Czeka 5 sekund = 1 godzina
        
        sem_lock(SEM_SHARED)
        aktualna_godzina++
        IF aktualna_godzina > 23:
            aktualna_godzina = 0
        
        // Reset o 5:00
        IF aktualna_godzina == 5:
            auta_w_serwisie = 0
            liczba_oczekujacych = 0
            pozar = 0
        
        // Otwarcie/zamknięcie
        IF godzina >= 8 AND godzina < 18 AND NOT pozar:
            serwis_otwarty = TRUE
            signal_serwis_otwarty()
        ELSE:
            serwis_otwarty = FALSE
        
        // Losowe sygnały do mechaników
        IF rand() % 100 < 10:
            pid_mechnika = get_random_mechanik()
            akcja = rand() % 4
            SWITCH akcja:
                CASE 0: kill(pid, SIGRTMIN)      // Zamknięcie
                CASE 1: kill(pid, SIGRTMIN+1)    // Przyspieszenie
                CASE 2: kill(pid, SIGRTMIN+2)    // Powolnienie
                CASE 3: kill(pid, SIGUSR1)       // Pożar
        
        sem_unlock(SEM_SHARED)
```

### 3. pracownik_serwisu.c (Obsługa klientów)

```pseudocode
PRACOWNIK_SERWISU(id):
    init_ipc(0)
    okienko_otwarty = FALSE
    liczba_okienek = 1  // Zawsze co najmniej jedno
    
    WHILE TRUE:
        msg = recv_msg(MSG_REJESTRACJA)
        IF msg.mtype == -1:
            BREAK
        
        kierowca_pid = msg.samochod.pid_kierowcy
        marka = msg.samochod.marka
        
        // Sprawdzenie marki
        IF NOT marka_obslugiwana(marka):
            msg.mtype = MSG_KIEROWCA(kierowca_pid)
            msg.samochod.zaakceptowano = FALSE
            send_msg(msg)
            CONTINUE
        
        // Pobranie usługi i wycena
        usluga = pobierz_usluge(msg.samochod.id_uslugi)
        msg.samochod.koszt = usluga.koszt
        msg.samochod.czas_naprawy = usluga.czas_wykonania
        msg.samochod.id_pracownika = id
        
        // Wysłanie wyceny
        msg.mtype = MSG_KIEROWCA(kierowca_pid)
        send_msg(msg)
        
        // Czekanie na decyzję
        msg = recv_msg(MSG_DECYZJA_USLUGI(id))
        
        IF NOT msg.samochod.zaakceptowano:
            CONTINUE
        
        // Mechanik pracuje na samochodzie
        stanowisko = znajdz_wolne_stanowisko(marka)
        IF stanowisko == -1:
            // Brak wolnych stanowisk - czekaj
            CONTINUE
        
        // Mechanik pracuje...
        // Czekanie na ukończenie
        msg = recv_msg(MSG_OD_MECHANIKA)
        
        // Sprawdzenie dodatkowych usterek
        IF msg.samochod.dodatkowa_usterka:
            msg.mtype = MSG_DECYZJA_DODATKOWA(id)
            send_msg(msg)
            msg = recv_msg(MSG_DECYZJA_DODATKOWA(id))
            
            IF msg.samochod.zaakceptowano:
                msg.samochod.czas_naprawy += msg.samochod.dodatkowy_czas
                msg.samochod.koszt += msg.samochod.dodatkowy_koszt
        
        // Wysłanie do kasy
        msg.mtype = MSG_KASA
        send_msg(msg)
        
        // Powiadomienie kierowcy o gotowości
        msg.mtype = MSG_KIEROWCA(kierowca_pid)
        send_msg(msg)
        
        // Dynamiczne zarządzanie okienkami
        liczba_czekajacych = sprawdz_dlugosc_kolejki()
        IF liczba_czekajacych > K2 AND liczba_okienek < 3:
            liczba_okienek = 3
        ELSE IF liczba_czekajacych <= 3 AND liczba_okienek == 3:
            liczba_okienek = 2
        ELSE IF liczba_czekajacych <= 2 AND liczba_okienek == 2:
            liczba_okienek = 1
```

### 4. mechanik.c (Naprawianie samochodów)

```pseudocode
MECHANIK(stanowisko_id):
    init_ipc(0)
    przyspieszone = FALSE
    zamknij = FALSE
    
    // Rejestracja handlerów sygnałów
    signal(SIGRTMIN, sig_zamknij)      // Zamknięcie
    signal(SIGRTMIN+1, sig_przyspiesz) // Przyspieszenie
    signal(SIGRTMIN+2, sig_normalnie)  // Powrót do normy
    signal(SIGUSR1, handle_pozar)      // Pożar
    
    WHILE TRUE:
        IF ewakuacja:
            BREAK
        
        // Czekanie na samochód
        msg = recv_msg(MSG_OD_PRACOWNIKA)
        
        samochod_pid = msg.samochod.pid_kierowcy
        czas_pracy = msg.samochod.czas_naprawy
        
        // Oznaczenie stanowiska jako zajętego
        sem_lock(SEM_SHARED)
        shared->stanowiska[stanowisko_id].zajete = 1
        shared->stanowiska[stanowisko_id].pid_mechanika = getpid()
        sem_unlock(SEM_SHARED)
        
        // Wykonanie pracy
        WYKONAJ_PRACE(czas_pracy):
            wykonano = 0
            WHILE wykonano < czas_pracy:
                IF ewakuacja:
                    BREAK
                
                sleep(0.1)
                IF przyspieszone:
                    wykonano += 0.2
                ELSE:
                    wykonano += 0.1
        
        // Sprawdzenie dodatkowych usterek (20%)
        IF rand() % 100 < 20:
            msg.samochod.dodatkowa_usterka = TRUE
            msg.samochod.id_dodatkowej_uslugi = rand() % MAX_USLUG
            usluga_dodatkowa = pobierz_usluge(msg.samochod.id_dodatkowej_uslugi)
            msg.samochod.dodatkowy_koszt = usluga_dodatkowa.koszt
            msg.samochod.dodatkowy_czas = usluga_dodatkowa.czas_wykonania
        
        // Informacja o ukończeniu
        msg.mtype = MSG_OD_MECHANIKA
        send_msg(msg)
        
        // Oznaczenie stanowiska jako wolnego
        sem_lock(SEM_SHARED)
        shared->stanowiska[stanowisko_id].zajete = 0
        IF zamknij:
            BREAK
        sem_unlock(SEM_SHARED)
```

### 5. kierowca.c (Klient serwisu)

```pseudocode
KIEROWCA():
    init_ipc(0)
    signal(SIGUSR1, handle_pozar)
    
    // Generowanie danych
    marka = chr('A' + rand() % 26)
    usluga_id = rand() % MAX_USLUG
    
    // Sprawdzenie obsługi marki
    IF NOT marka_obslugiwana(marka):
        RETURN
    
    // Czekanie na otwarcie
    WHILE NOT serwis_otwarty:
        IF ewakuacja:
            RETURN
        
        czas_do_otwarcia = GODZINA_OTWARCIA - biezaca_godzina
        usluga = pobierz_usluge(usluga_id)
        
        IF NOT (usluga.krytyczna OR czas_do_otwarcia <= LIMIT_OCZEKIWANIA):
            RETURN
        
        wait_serwis_otwarty()
    
    // Rejestracja
    msg.mtype = MSG_REJESTRACJA
    msg.samochod.pid_kierowcy = getpid()
    msg.samochod.marka = marka
    msg.samochod.id_uslugi = usluga_id
    
    sem_lock(SEM_SHARED)
    shared->liczba_oczekujacych_klientow++
    sem_unlock(SEM_SHARED)
    
    send_msg(msg)
    signal_nowa_wiadomosc()
    
    // Czekanie na wycenę
    WHILE TRUE:
        msg = recv_msg(MSG_KIEROWCA(getpid()), IPC_NOWAIT)
        IF msg != NULL:
            BREAK
        wait_nowa_wiadomosc(TIMEOUT)
    
    koszt = msg.samochod.koszt
    czas = msg.samochod.czas_naprawy
    id_pracownika = msg.samochod.id_pracownika
    
    // Decyzja (2% szans na rezygnację)
    zaakceptuj = (rand() % 100 >= 2)
    
    msg.mtype = MSG_DECYZJA_USLUGI(id_pracownika)
    msg.samochod.zaakceptowano = zaakceptuj
    send_msg(msg)
    
    IF NOT zaakceptuj:
        RETURN
    
    // Czekanie na naprawę i sprawdzenie dodatkowych usterek
    msg = recv_msg(MSG_DECYZJA_DODATKOWA(id_pracownika), IPC_NOWAIT)
    IF msg != NULL AND msg.samochod.dodatkowa_usterka:
        // 20% szans na odrzucenie rozszerzenia
        rozszerz = (rand() % 100 >= 20)
        
        msg.mtype = MSG_DECYZJA_DODATKOWA(id_pracownika)
        msg.samochod.zaakceptowano = rozszerz
        send_msg(msg)
    
    // Czekanie na ukończenie
    WHILE TRUE:
        msg = recv_msg(MSG_KIEROWCA(getpid()), IPC_NOWAIT)
        IF msg != NULL:
            BREAK
        wait_nowa_wiadomosc(TIMEOUT)
    
    // Płatność
    msg.mtype = MSG_KASA
    send_msg(msg)
    
    // Czekanie na potwierdzenie płatności
    msg = recv_msg(MSG_POTWIERDZENIE_PLATNOSCI(id_pracownika))
    
    RETURN
```

### 6. kasjer.c (Obsługa płatności)

```pseudocode
KASJER():
    init_ipc(0)
    signal(SIGUSR1, handle_pozar)
    
    dzienny_utarg = 0
    
    WHILE TRUE:
        wait_serwis_otwarty()
        
        IF ewakuacja:
            BREAK
        
        WHILE TRUE:
            IF ewakuacja:
                BREAK
            
            msg = recv_msg(MSG_KASA)
            
            // Przyjęcie płatności
            koszt = msg.samochod.koszt
            
            IF msg.samochod.dodatkowa_usterka:
                koszt += msg.samochod.dodatkowy_koszt
            
            dzienny_utarg += koszt
            
            // Potwierdzenie płatności
            msg.mtype = MSG_POTWIERDZENIE_PLATNOSCI(msg.samochod.id_pracownika)
            send_msg(msg)
            
            sem_lock(SEM_SHARED)
            shared->liczba_oczekujacych_klientow--
            sem_unlock(SEM_SHARED)
        
        // Zapis raportu dziennego
        zapisz_raport("Dzienny utarg: " + str(dzienny_utarg) + " PLN")
        dzienny_utarg = 0
```

---

## Podsumowanie

Projekt Serwis Samochodów demonstruje zaawansowaną synchronizację procesów w systemach uniksowych. Kluczowe aspekty to:

1. **Poprawna obsługa IPC**: Prawidłowe użycie pamięci dzielonej, kolejek komunikatów i semaforów

2. **Dynamiczność systemu**: System automatycznie dostosowuje się do zmian obciążenia (otwieranie/zamykanie okienek)

3. **Solidne obsługa sygnałów**: Wszystkie sygnały obsługiwane bezpiecznie bez przerywania operacji krytycznych

4. **Realistyczne zachowanie**: Symulacja zawiera elementy losowości (decyzje kierowców, dodatkowe usterki) odzwierciedlające rzeczywiste warunki

5. **Kompletne logowanie**: Wszystkie zdarzenia są zarejestrowane dla celów audytu i debugowania
