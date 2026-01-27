
# Serwis samochodów (Temat 6)

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

* **kierownik.c** - Proces nadrzędny sterujący czasem symulacji, otwieraniem i zamykaniem serwisu, losowymi zdarzeniami do mechaników oraz ewakuacją (pożar). Inicjuje IPC i czyści zasoby przy końcu pracy.

* **mechanik.c** - Symuluje pracę mechaników na stanowiskach. Obsługuje naprawy, zgłasza dodatkowe usterki, reaguje na sygnały od kierownika.

* **pracownik_serwisu.c** - Proces obsługi klienta. Działa wielowątkowo - każdy wątek obsługuje jednego klienta. Dynamicznie otwiera dodatkowe okienka w zależności od długości kolejki oczekujących. Przydziela samochody do wolnych mechaników.

* **kasjer.c** - Obsługuje finalizację usługi, pobiera opłaty, generuje raporty finansowe i loguje transakcje.

* **kierowca.c** - Symuluje klienta. Losuje markę i usługę, rejestruje się, czeka na wycenę, decyduje o naprawie lub rezygnacji, czeka na naprawę, decyduje o dodatkowych usterkach i płaci.

---

* **generator.c** - Proces generujący wielu kierowców z limitem aktywnych dzieci. Posiada wątek zbierający procesy zombie.
* **serwis_ipc.c / serwis_ipc.h** - wspólne definicje IPC, struktury danych, semafory, kolejki, logowanie i cennik usług.

---
###  Z czym były problemy



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

### wyróżniające elementy:

Nie wiem


### Testy




## Pseudokody kluczowych procesów


### kierownik.c

```

init_ipc()

setpgid()

while running:

wait(SEC_PER_H)

aktualizuj_godzine()

if pozar: serwis_otwarty = 0

if w_oknie_godzin: otworz_serwis()

else: zamknij_serwis()

losowo wyslij sygnaly do mechanikow

if shutdown:

wyslij SIGTERM do grupy

cleanup_ipc()

```

  

### pracownik_serwisu.c

```

while serwis dziala:

czekaj_na_otwarcie

zarzadzaj_liczba_okienek(K1, K2)

odbierz zgloszenia kierowcow

wycen usluge i odeślij

pobierz decyzje kierowcy

przydziel wolnego mechanika

obsluz dodatkowe usterki

po zakonczeniu naprawy -> kasa

```

  

### mechanik.c

```

czekaj_na_otwarcie

while serwis otwarty:

if zamknij_po: zakoncz i wyjdz

odbierz zlecenie

napraw (z przyspieszeniem jesli aktywne)

losowo zglos dodatkowa usterke

po zakonczeniu -> powiadom pracownika

```

  

### kierowca.c

```

losuj marke i usluge

czekaj_na_otwarcie (jesli krytyczna lub krotki czas)

wyslij rejestracje

odbierz wycene

zaakceptuj lub odrzuc

czekaj na zakonczenie naprawy

jesli dodatkowa usterka -> decyzja

po platnosci odbierz kluczyki

```

  ---