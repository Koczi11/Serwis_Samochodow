
# Serwis samochodów

**Imię:** Kacper
**Nazwisko:** Koczera
**Numer Albumu:** 155191
**GitHub:** https://github.com/Koczi11/Serwis_Samochodow

--- 

Wymagania projektu i szczegółowy opis są dostępne w pliku: https://github.com/Koczi11/Serwis_Samochodow/blob/main/README.md

---
### Pliki i ich działanie w projekcie

Projekt zrealizowano w C z użyciem System V IPC (pamięć dzielona, semafory, kolejki komunikatów). Kompilacja przez Makefile.

### Budowa i uruchomienie

1. make
2. ./kierownik
3. ./pracownik_serwisu
4. ./mechanik
5. ./kasjer
6. ./generator [liczba_kierowców]

---

### Główne procesy


* **kierownik.c** - Proces nadrzędny. Inicjuje IPC, prowadzi zegar symulacji, otwiera/zamyka serwis, wywołuje zdarzenia losowe (w tym pożar), a na końcu sprząta zasoby.
* **mechanik.c** - Proces stanowiska naprawy. Odbiera zlecenia, symuluje czas pracy (może być przyspieszony), obsługuje dodatkowe usterki i reaguje na sygnały zamknięcia/ewakuacji.
* **pracownik_serwisu.c** - 3 procesy recepcji. Każdy aktywny proces tworzy osobny wątek na obsługę kierowcy. Dodatkowe okienka są aktywowane/wyłączane zależnie od długości kolejki (progi K1/K2).
* **kasjer.c** - Obsługuje płatności, potwierdza transakcje i zapisuje raport dzienny.
* **kierowca.c** - Symuluje klienta: losuje markę i usługę, czeka na otwarcie (jeśli usługa krytyczna lub krótki czas oczekiwania), przechodzi wycenę, decyzje i płatność.
* **generator.c** - Tworzy procesy `kierowca`. Ma wątek „reaper” do sprzątania procesów potomnych.
* **serwis_ipc.c / serwis_ipc.h** - Definicje IPC, struktur danych i funkcji pomocniczych (semafory, kolejki, bezpieczne oczekiwanie).

 ---
 
###  Z czym były problemy?
* Najtrudniejsza była obsługa **ewakuacji (pożaru)**. Z każdą zmianą w kodzie system działania pożaru miał nowe problemy, które trzeba było naprawiać. Myślę, że 70% czasu spędzonego nad projektem przeznaczyłem na obsługę tego sygnału.
* Kolejnym problemem był błąd jaki popełniłem. czyli synchronizowanie projektu za pomocą sleep()/usleep(). Po ciężkiej przebudowie w kodzie udało się go wyeliminować.

---
### Komunikacja między-procesowa

1.  **Pamięć dzielona** (SharedData) - globalny stan serwisu:
	* aktualna godzina symulacji
	* status otwarcia
	* informacje o pożarze i resecie po pożarze
	* liczba aut w serwisie oraz liczba oczekujących
	* status stanowisk mechaników
	* PID-y procesów (kierownik, kasjer, pracownicy, generator)

2.  **Kolejki komunikatów (System V Message Queues)**
	* kolejka kierowców (rejestracja, decyzje, odpowiedzi)
	* kolejka mechaników (zlecenia napraw i zdarzenia)
	* kolejka kasjera (płatności i potwierdzenia)
	* odpowiedzi kierowane po `mtype` zależnym od PID (uniknięcie kolizji)

3.  **Semafory (System V)**
	* `SEM_SHARED` – globalna ochrona awaryjna
	* `SEM_STANOWISKA` – ochrona tablicy stanowisk
	* `SEM_LICZNIKI` – ochrona liczników
	* `SEM_STATUS` – ochrona statusu serwisu
	* `SEM_TIMER` – bezpieczne oczekiwanie z timeoutem (`semtimedop`)


---
### Model synchronizacji

* Pamięć dzielona chroniona semaforami binarnymi.
* Komunikacja asynchroniczna przez kolejki z rozdzieleniem typów wiadomości na podstawie PID.
* Częste użycie `IPC_NOWAIT` + pętle z obsługą `EINTR`, aby sygnały nie blokowały procesu.
* Procesy dołączają do grupy kierownika, co ułatwia globalne sygnały ewakuacji.

---

### Czas

* Czas symulacji aktualizowany przez kierownika co `SEC_PER_H` sekund (domyślnie 5s = 1h).
* Dzień startuje od 6:00, otwarcie o 8:00, zamknięcie o 18:00.

---

### Sygnały


* **SIGRTMIN** – zamknięcie stanowiska mechanika po zakończeniu bieżącej naprawy.
* **SIGRTMIN+1** – przyspieszenie pracy stanowiska (tryb szybszy).
* **SIGRTMIN+2** – powrót do normalnego tempa pracy.
* **SIGUSR1** – pożar i ewakuacja procesów w grupie (generator go ignoruje).
* **SIGINT/SIGTERM** – bezpieczne zatrzymanie symulacji przez kierownika.

---

## Testy
### Test 1
Sprawdzenie przepustowości kolejki komunikatów oraz wydajności wątków pracownika serwisu przy dużej liczbie odrzuceń. 
* Ustawiamy generator na 5000 kierowców. 
	* Sprawdzamy czy kolejka komunikatów nie ulegnie przepełnieniu, co mogłoby zablokować generator lub spowodować błędy.
	* Sprawdzamy czy pracownicy serwisu poprawnie odsyłają komunikaty.

---

* Generator poprawnie stworzył kierowców i zakończył działanie.
![screen1](/screens/e96938f4-29d9-495e-aca6-47d4ca2aa41d.png)
![screen2](/screens/95b26fd4-6558-43e5-adfb-f057a848e3c1.png)
* Pracownicy serwisu poprawnie obsługują klientów. Odsyłają 3841 kierowców z nieobsługiwaną marką.
![screen3](/screens/aa028589-2753-48b4-be80-f5e37b1dd791.png)
![screen4](/screens/d7331c5b-554d-4bd9-86c2-ce56105411b7.png)
![screen5](/screens/6bbd3270-4db4-4ff2-88df-fb31e399a7d5.png)

	Generator zakończył pracę pomyślnie, nie zgłaszając błędów dostępu do IPC. Wszystkie procesy potomne zostały poprawnie posprzątane przez wątek - brak procesów zombie, a kolejki komunikatów zostały całkowicie opróżnione.
**Test zaliczony**

---


### Test 2
Sprawdzenie poprawności działania semaforów SEM_STANOWISKA przy wielu pracownikach obsługi próbujących jednocześnie przedzielić zadanie.
* Uruchamiamy proces pracownika serwisu tak, aby działały wszystkie 3 procesy.
* Duży napływ klientów obsługiwanych marek.
	* Sprawdzamy czy semafor poprawnie wpuszcza tylko jednego pracownika do funkcji znajdz_wolne_stanowisko.
	* Sprawdzamy czy nie występuje błąd przypisania dwóch różnych aut do tego samego mechanika.
	* Sprawdzamy czy komunikaty w kolejce nie są nadpisywane.

---

* Uruchomiono 3 równoległe procesy pracownika serwisu i ustawiono generator na wysyłanie wyłącznie obsługiwanych marek.
![screen6](/screens/2f26fea3-6fa6-4204-b540-815e82f87f0d.png)
* Pracownicy serwisu poprawnie przypisują samochody na dane stanowiska.
![screen7](/screens/11bf7390-a787-4dfb-aa30-cd7e09696933.png)

Semafor SEM_STANOWISKA prawidłowo wpuszcza jednego pracownika serwisu, który sprawdza tablicę. Brak podwójnych przypisań. Komunikaty w kolejce nie są nadpisywane.
**Test zaliczony**

---

### Test 3
Sprawdzenie, czy nagłe przerwanie operacji IPC sygnałem pożaru nie zawiesza systemu.
* Pełne obłożenie serwisu i manualne wysłanie sygnału.
* Procesy znajdują się w różnych stanach.
	* Sprawdzamy czy po ewakuacji symulacja działa poprawnie.
	* Sprawdzamy czy semafory nie zostały zablokowane i czy klienci uciekający z serwisu poprawnie zwalniają miejsce w pamięci.

---
* W trakcie działania symulacji kierownik wysyła syngał pożaru. Procesy poprawnie go obsługują, uciekają z serwisu.
![screen8](/screens/be9c515f-da17-460b-a8ac-5712cf61a503.png)
![screen9](/screens/160a731e-a4b8-40d4-a67e-f71f7fdf2e5c.png)
![screen10](/screens/176736e7-f03c-4501-b074-8b16e672becb.png)
![screen11](/screens/fb45d695-09a0-4b7f-9046-c0ea7a250f1f.png)
![screen12](/screens/e36f785a-84a5-4b5a-bb6b-be2a889695cd.png)

* Symulacja prawidłowo resetuje stan pożaru o 5:00. Dzięki temu o 8:00 serwis wznawia swoją pracę.
![screen13](/screens/6bd40d1a-bd2f-4ee1-a2e1-76dfd31cce3d.png)

System wykazał pełną odporność na nagłe przerwanie sygnałem. Nie wystąpiły zakleszczenia, a pamięć współdzielona została poprawnie przywrócona do spójności.
**Test zaliczony**

---
### Test 4
Sprawdzenie czy pojedynczy proces kasjera nie blokuje pracy serwisu.
* Duża liczba klientów i bardzo krótkie naprawy.
	* Sprawdzamy czy kolejka kasjera jest opróżniana na bieżąco, a transakcje są poprawnie logowane mimo dużego natężenia ruchu.

---
* Wszystkie naprawy mechanika zostały drastycznie przyspieszone w celu przeprowadzenia testu.
![screen14](/screens/68e8305d-9344-4adc-a7ed-81967e390c06.png)
![screen15](/screens/cf2914a8-cf4c-462b-bcfc-10c1f0ee860a.png)
![screen16](/screens/d5caba20-a085-4714-aabd-6d81053f1f87.png)

Proces kasjera bez problemu obsłużył wszystkie nadchodzące komunikaty. Kolejka komunikatów była opróżniana na bieżąco, a wszystkie transakcje zostały poprawnie zalogowane. Nie zaobserwowano zatorów, ani oczekiwania innych procesów.
**Test zaliczony**


---

## Linki do kluczowych fragmentów (wymagane funkcje systemowe)

a. **Tworzenie i obsługa plików** (`creat()`, `open()`, `close()`, `read()`, `write()`, `unlink()`):
* `open()`/`write()`/`close()` w logowaniu do plików: https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/serwis_ipc.c#L338-L354

b. **Tworzenie procesów** (`fork()`, `exec()`, `exit()`, `wait()`):
* `fork()`/`execl()`/`exit()`/`wait()` w uruchamianiu stanowisk mechaników: https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/mechanik.c#L109-L124

c. **Tworzenie i obsługa wątków**:
* `pthread_create()`/`pthread_detach()` (obsługa klientów): https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/pracownik_serwisu.c#L594-L596
* `pthread_create()`/`pthread_detach()`/`pthread_mutex_lock()`/`pthread_mutex_unlock()`/`pthread_cond_wait()`/`pthread_cond_broadcast()` (wątek reaper): https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/generator.c#L159-L186

d. **Obsługa sygnałów** (`kill()`, `raise()`, `signal()`, `sigaction()`):
* `signal()` (ignorowanie SIGTERM/SIGINT): https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/kierownik.c#L42-L47
* `kill()` (sygnały do grup i procesów): https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/kierownik.c#L52-L114
* `sigaction()` (konfiguracja obsługi sygnałów): https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/kierownik.c#L196-L214

e. **Synchronizacja procesów (wątków)** (`ftok()`, `semget()`, `semctl()`, `semop()`):
* `ftok()`/`semget()`/`semctl()` (inicjalizacja semaforów): https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/serwis_ipc.c#L33-L144
* `semop()` (blokowanie/odblokowanie): https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/serwis_ipc.c#L289-L331

g. **Segmenty pamięci dzielonej** (`ftok()`, `shmget()`, `shmat()`, `shmdt()`, `shmctl()`):
* `ftok()`/`shmget()`/`shmat()` (tworzenie i mapowanie): https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/serwis_ipc.c#L33-L84
* `shmdt()`/`shmctl()` (odłączanie i usuwanie): https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/serwis_ipc.c#L228-L237

h. **Kolejki komunikatów** (`ftok()`, `msgget()`, `msgsnd()`, `msgrcv()`, `msgctl()`):
* `ftok()`/`msgget()` (tworzenie kolejek): https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/serwis_ipc.c#L33-L166
* `msgsnd()`/`msgrcv()` (wysyłanie/odbieranie): https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/serwis_ipc.c#L433-L455
* `msgctl()` (usuwanie kolejek): https://github.com/Koczi11/Serwis_Samochodow/blob/main/src/serwis_ipc.c#L255-L269

---


## Pseudokody kluczowych procesów
### Kierownik
```
init_ipc(is_parent=1)
setpgid(0,0)
init shared: godzina=6, serwis_otwarty=0
while running:
	safe_wait_seconds(SEC_PER_H)
	godzina++ (wrap 0..23)
	if godzina==5: reset po pożarze
	if godzina w [8,18): otwórz serwis i wyślij MSG_CTRL_OPEN_*
	else: zamknij serwis
	if serwis_otwarty and los<10%:
		losowe: SIGRTMIN / SIGRTMIN+1 / SIGRTMIN+2 / SIGUSR1
on shutdown: SIGTERM do grupy, cleanup_ipc()
```

### Pracownik serwisu (wątek klienta)
```
on MSG_REJESTRACJA:
	if marka nieobsługiwana -> odeślij negatywną wycenę
	wyceń usługę i wyślij do kierowcy
	czekaj na decyzję (MSG_DECYZJA_USLUGI_PID)
	jeśli akceptacja:
		przydziel mechanika (SEM_STANOWISKA)
		przekazuj zdarzenia od mechanika
		obsłuż dodatkową usterkę (MSG_DECYZJA_DODATKOWA_PID)
		wyślij do kasjera i czekaj na potwierdzenie płatności
		zakończ
```

### Mechanik
```
zarejestruj PID stanowiska
while true:
	czekaj na MSG_CTRL_OPEN_MECHANIK
	reset stanowiska
	while serwis_otwarty:
		jeśli SIGUSR1: ewakuuj
		jeśli SIGRTMIN: zamknij po bieżącej naprawie
		odbierz zlecenie naprawy (mtype=100+id)
		wykonaj_prace(czas) z uwzględnieniem przyspieszenia
		jeśli dodatkowa usterka -> wyślij do pracownika
		po zakończeniu -> powiadom pracownika
```

### Kasjer
```
zarejestruj PID
while true:
	czekaj na MSG_CTRL_OPEN_KASJER
	dzienny_utarg=0
	while serwis_otwarty lub są klienci:
		jeśli MSG_KASA: pobierz opłatę, wyślij potwierdzenie
		jeśli SIGUSR1: przerwij dzień
```

### Kierowca
```
wylosuj markę i usługę
czekaj na otwarcie (lub odjedź jeśli nie warto czekać)
wyślij MSG_REJESTRACJA
odbierz wycenę i podejmij decyzję
jeśli akceptacja: czekaj na naprawę
obsłuż ewentualną dodatkową usterkę
zapłać w kasie i odbierz auto
```

### Generator
```
uruchom wątek reaper (SIGCHLD)
for i in 1..N:
	jeśli aktywnych >= limit -> czekaj
	fork() -> exec kierowca
czekaj aż wszystkie dzieci się zakończą
```