
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
6. ./generator [liczba_kierowców] [max_aktywnych]

---

### Główne procesy


* **kierownik.c** - Proces nadrzędny sterujący czasem symulacji, otwieraniem i zamykaniem serwisu. Zarządza losowymi zdarzeniami oraz sytuacjami awaryjnymi (pożar). Inicjuje zasoby IPC i czyści je po zakończeniu symulacji. 
* **mechanik.c** - Symuluje pracę mechanika na konkretnym stanowisku. Odbiera zlecenia naprawy, symuluje czas pracy (z uwzględnieniem przyspieszeń), losowo zgłasza dodatkowe usterki i komunikuje się z pracownikiem serwisu.
* **pracownik_serwisu.c** - Proces obsługi klienta (recepcja). Działa w modelu hybrydowym (procesy + wątki) – dla każdego klienta tworzony jest osobny wątek, co pozwala na równoległą obsługę wielu kierowców. Dynamicznie otwiera dodatkowe "okienka" (aktywuje nieaktywne procesy) w zależności od długości kolejki oczekujących. 
* **kasjer.c** - Obsługuje finalizację usługi. Odbiera informacje o kosztach, pobiera opłaty od kierowców, generuje raporty finansowe i loguje transakcje.
* **kierowca.c** - Symuluje klienta. Losuje markę samochodu i rodzaj usługi. Przechodzi pełną ścieżkę: rejestracja -> oczekiwanie na wycenę -> decyzja (akceptacja/odrzucenie) -> oczekiwanie na naprawę -> decyzja o dodatkowej usterce -> płatność -> odbiór auta. 
* **generator.c** - Proces odpowiedzialny za masowe tworzenie procesów `kierowca`. Posiada dedykowany wątek do bieżącego usuwania martwych procesów potomnych, aby nie zapchać tablicy procesów w systemie. 
* **serwis_ipc.c / serwis_ipc.h** - Biblioteka współdzielona zawierająca definicje kluczy IPC, struktur danych, semaforów oraz funkcji pomocniczych.

 ---
 
###  Z czym były problemy
* Największym wyzwaniem było zapewnienie poprawnej synchronizacji przy **ewakuacji (pożarze)**. Sygnał `SIGUSR1` jest wysyłany do całej grupy procesów, co wymagało, aby każdy proces w dowolnym momencie potrafił przerwać działanie, zwolnić zasoby i bezpiecznie się zakończyć. Problem często wracał z minimalnymi zmianami w kodzie, dlatego bez dwóch zdań powodowało to najwięcej problemów

---
### Komunikacja między-procesowa

1.  **Pamięć dzielona** (SharedData) -  globalny stan serwisu:
	* aktualna godzina symulacji
	* status otwarcia
	* informacje o pożarze i resecie po pożarze
	* liczba aut w serwisie oraz liczba oczekujących
	* status stanowisk mechaników

2.  **Kolejki komunikatów (System V Message Queues)**

	* kolejka kierowców (rejestracja, decyzje, odpowiedzi)

	* kolejka mechaników (zlecenia napraw i zdarzenia)

	* kolejka kasjera (płatności i potwierdzenia)

3.  **Semafory (System V)**

	*  `SEM_SHARED` – globalna ochrona

	*  `SEM_STANOWISKA` – ochrona tablicy stanowisk

	*  `SEM_LICZNIKI` – ochrona liczników

	*  `SEM_STATUS` – ochrona statusu serwisu

	*  `SEM_TIMER` – kontrola oczekiwania z timeoutem


---
### Model synchronizacji

* Pamięć dzielona chroniona semaforami binarnymi.

* Kolejki komunikatów zapewniają bezpieczną wymianę danych między procesami.

* Każdy kierowca identyfikowany przez PID, wykorzystywany do unikalnych `mtype` w odpowiedziach.

---

### Czas

* Czas symulacji aktualizowany przez kierownika w krokach co `SEC_PER_H` sekund (domyślnie 5s = 1h symulacji).

* Na podstawie godziny następuje otwarcie i zamknięcie serwisu.

---

### Sygnały


*  **SIGRTMIN** – zamknięcie stanowiska mechanika po zakończeniu bieżącej naprawy.

*  **SIGRTMIN+1** – przyspieszenie pracy stanowiska o 50% (jednorazowe).

*  **SIGRTMIN+2** – powrót do normalnego tempa pracy.

*  **SIGUSR1** – pożar i ewakuacja wszystkich procesów.

*  **SIGINT/SIGTERM** – bezpieczne zatrzymanie całej symulacji przez kierownika.

---

## Linki do kluczowych fragmentów
cos tam



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




## Pseudokody kluczowych procesów

