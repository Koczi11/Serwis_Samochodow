# Serwis samochodów (Temat 6)

**Imię:** Kacper
**Nazwisko:** Koczera
**Numer Albumu:** 155191
**GitHub:** https://github.com/Koczi11/Serwis_Samochodow

## Opis zadania

W pewnej miejscowości znajduje się serwis samochodów dostępny w godzinach $\text{od } T_p \text{ do } T_k$. Serwis obsługuje tylko samochody marek: A, E, I, O, U i Y. Pozostałe marki – z zakresu od A do Z (łącznie 26 różnych marek) nie są obsługiwane. W serwisie znajduje się 8 stanowisk do naprawy pojazdów, przy czym na stanowiskach 1-7 możliwa jest naprawa marek A, E, I, O, U i Y, natomiast na stanowisku 8 możliwa jest naprawa tylko marek U i Y.

## Opis działania serwisu

Samochody (marki z zakresu A-Z) pojawiają się w serwisie w losowej chwili (nawet poza godzinami otwarcia) i są obsługiwane/serwisowane według następujących zasad:
- Serwis obsługuje tylko samochody marek: A, E, I, O, U i Y.
- Jeżeli samochód przyjedzie poza godzinami pracy może czekać w kolejce (jeżeli usterka krytyczna – określić 3 takie naprawy lub czas do otwarcia krótszy niż T1).
- Czas naprawy każdego z pojazdów ustalany jest indywidualnie przez pracownika serwisu (obsługa klienta) - kierowca/właściciel podaje zakres napraw – pracownik serwisu określa przybliżony czas naprawy oraz przewidywany koszt naprawy określony w oparciu o cennik (co najmniej 30 usług).
- Kierowca musi zaakceptować warunki naprawy (szacowany czas i kwotę) – ok. 2% kierowców nie akceptuje warunków i odjeżdża z serwisu bez naprawy.
- W ok. 20% przypadków podczas diagnostyki na hali wychodzą dodatkowe usterki, kwalifikujące się do naprawy – mechanik przekazuje te informacje do pracownika serwisu, który komunikuje się z kierowcą i ustalają czy dodatkowe prace serwisowe mają być wykonane czy nie. Ok. 20% kierowców nie zgadza się na rozszerzenie pierwotnego zakresu napraw. Rozszerzenie zakresu może (nie musi) wydłużyć czas naprawy.
- Po zakończeniu naprawy mechanik przekazuje do pracownika serwisu formularz z zakresem wykonanych napraw, pracownik serwisu ustala kwotę za usługę i informuje klienta o możliwości odbioru samochodu.
- Klient po uiszczeniu opłaty w kasie odbiera kluczyki od pracownika serwisu i opuszcza serwis.

Stanowisko bezpośredniej obsługi kierowców - pracownik serwisu, działa dodatkowo według następujących reguł:
- W serwisie są 3 stanowiska obsługi klientów, zawsze działa min. 1 stanowisko.
- Jeżeli w kolejce do rejestracji stoi więcej niż K1 kierowców (K1>=3) otwiera się drugie stanowisko obsługi. Drugie stanowisko zamyka się jeżeli liczba klientów w kolejce jest mniejsza niż lub równa 2.
- Jeżeli w kolejce do rejestracji stoi więcej niż K2 kierowców (K2>=5) otwiera się trzecie stanowisko obsługi. Trzecie stanowisko zamyka się jeżeli liczba klientów w kolejce jest mniejsza niż lub równa 3.

Sygnały kierownika serwisu:
- Kierownik serwisu po wysłaniu sygnału1 (do mechanika) może zamknąć dowolne stanowisko napraw – jeżeli w momencie otrzymania sygnału mechanik był w trakcie obsługi samochodu, kończy jego naprawę wg ustalonego spisu i zamyka stanowisko. Kolejne/oczekujące samochody zostają przekierowane na inne stanowiska.
- Kierownik serwisu po wysłaniu sygnału2 (do mechanika) może przyspieszyć czas naprawy samochodów na danym stanowisku o 50% – próba kolejnego przyśpieszenia ma być ignorowana. Przyśpieszyć można tylko proces, który pracuje w trybie normalnym.
- Kierownik serwisu po wysłaniu sygnału3 (do mechanika) może przywrócić czas naprawy samochodów na danym stanowisku do stanu pierwotnego – sygnał3 może być przyjęty tylko wtedy, gdy wcześniej proces otrzymał sygnał2.
- Kierownik serwisu po wysłaniu sygnału4 (pożar) zamyka cały serwis – mechanicy przerywają pracę, wszyscy opuszczają serwis.


## Wymagane procedury
- **Kierownik**
-  **Pracownik serwisu**
-  **Mechanik**
-  **Kasjer**
-  **Kierowca**

## Plan testów
#### 1. Obsługa nieobsługiwanych marek i kolejek po godzinach pracy
Symulujemy przyjazd 20 samochodów różnych marek (od A do Z), z czego 8 to marki obsługiwane, a 12 to marki nieobsługiwane. Wszystkie samochody przyjeżdżają np. 30 min przed otwarciem, ale 3 mają usterkę krytyczną.

Oczekujemy:
- Pojazdy nieobsługiwanych marek zostaną natychmiast odrzucone.
- Pojazdy obsługiwane z krytyczną usterką mogą czekać. Pozostałe czekają tylko jeśli do otwarcia < T1, inaczej odjeżdżają.
- Brak zakleszczeń.

#### 2. Zmiana liczby stanowisk rejestracji klientów
Generujemy napływ 15 kierowców w krótkim czasie. Początkowo działa 1 stanowisko. Po pojawieniu się K1 klientów otwiera się 2 stanowisko, K2 klientów otwiera 3 stanowisko. Następnie liczba kierowców spada, co powinno spowodować zamykanie stanowisk.

Oczekujemy:
- System dynamicznie otwiera stanowisko 2 i 3 zgodnie z progami.
- System ponownie zamyka stanowiska 3, 2 zgodnie z progami.
- Obsługa klientów odbywa się równolegle.
- Brak zakleszczeń.

#### 3. Dodatkowe usterki podczas diagnostyki i akceptacja zakresu
Symulujemy przyjazd 10 samochodów różnych marek obsługiwanych. 20% - wykryte dodatkowe usterki, wśród nich 20% kierowców odmawia rozszerzenia zakresu. Mechanik zgłasza nowe usterki do pracownika serwisu, który kontaktuje się z kierowcą.

Oczekujemy:
- Mechanik poprawnie zgłasza dodatkową usterkę.
- Pracownik serwisu poprawnie prosi o zgodę kierowcę.
- Kierowca, zgodnie z prawdopodobieństwem, podejmuje decyzję.
- Czas naprawy zmienia się tylko jeśli kierowca zaakceptuje dodatkową usterkę.
-  Brak zakleszczeń.

#### 4. Sygnały kierownika
Uruchomionych jest 8 stanowisk i 12 samochodów do naprawy. Wysyłamy sygnał2, aby przyśpieszyć prace stanowiska 3. Po chwili ponownie wysyłamy sygnał2, który powinien zostać zignorowany. Za pomocą sygnału3 - powrót stanowiska 3 do normalnej szybkości. Sygnałem1 zamykamy stanowisko 5 po zakończeniu bieżącej naprawy. Sygnał4 - pożar, natychmiastowe przerwanie pracy całego serwisu.

Oczekujemy:
- Tylko jedno przyspieszenie jest możliwe, kolejne ignorowane.
- Po sygnale3 prędkość wraca do normy.
- Stanowisko 5 kończy bieżącą naprawę i zamyka się.
- Sygnał4 kończy pracę wszystkich procesów.
