# Serwis samochodów

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
-  **Kierownik**
-  **Pracownik serwisu**
-  **Mechanik**
-  **Kasjer**
-  **Kierowca**

<br>

--- 

## English Version

# Car Service

## Task Description

In a certain town, there is a car service center open $\text{from } T_p \text{ to } T_k$. The service center exclusively handles cars of the following brands: A, E, I, O, U and Y. The remaining brands – from the A to Z range (26 different brands in total) – are not serviced. The service center has 8 repair bays. Repair bays 1-7 can service brands A, E, I, O, U and Y, while repair bay 8 is strictly reserved for servicing only brands U and Y.

## Service Operation Details

Cars (brands from the A-Z range) arrive at the service center at random times (even outside of opening hours) and are processed/serviced according to the following rules:
- The service center only accepts car brands: A, E, I, O, U and Y.
- If a car arrives outside of working hours, it can wait in the queue (if it has a critical fault – you must define 3 such repairs, or if the time remaining until opening is less than T1).
- The repair time for each vehicle is determined individually by a service advisor (customer service) - the driver/owner provides the scope of repairs and the service advisor determines the approximate repair time and estimated cost based on a price list (which must include at least 30 services).
- The driver must accept the repair terms (estimated time and cost) – approx. 2% of drivers do not accept the terms and leave the service center without getting repairs.
- In approx. 20% of cases, additional faults qualifying for repair are discovered during diagnostics on the workshop floor. The mechanic passes this information to the service advisor, who contacts the driver to agree on whether the additional service work should be performed. Approx. 20% of drivers do not agree to expand the original scope of repairs. Expanding the scope may (but does not have to) extend the repair time.
- After the repair is completed, the mechanic hands over a form with the scope of completed repairs to the service advisor. The service advisor determines the final price for the service and informs the customer that the car is ready for pickup.
- After paying the fee at the cashier, the customer retrieves the keys from the service advisor and leaves the service center.

Customer service desks (service advisors) operate according to the following additional rules:
- There are 3 customer service desks in the center and at least 1 desk is always open.
- If there are more than K1 drivers in the registration queue (K1 >= 3), a second service desk is opened. The second desk is closed if the number of customers in the queue drops to 2 or fewer.
- If there are more than K2 drivers in the registration queue (K2 >= 5), a third service desk is opened. The third desk is closed if the number of customers in the queue drops to 3 or fewer.

Service Manager Signals:
- **Signal 1 (to the mechanic):** The service manager can close any repair bay. If the mechanic was in the middle of servicing a car when receiving the signal, they finish the current repair according to the agreed list and then close the bay. The next/waiting cars are redirected to other bays.
- **Signal 2 (to the mechanic):** The service manager can speed up the repair time of cars at a given bay by 50%. Any attempt to speed it up further must be ignored. The process can only be sped up if it is operating in normal mode.
- **Signal 3 (to the mechanic):** The service manager can restore the car repair time at a given bay to its original state. Signal 3 can only be accepted if the process previously received Signal 2.
- **Signal 4 (Fire):** The service manager closes the entire service center. Mechanics stop their work immediately and everyone evacuates the premises.


## Required Procedures
-  **Manager (Kierownik)**
-  **Service Advisor (Pracownik serwisu)**
-  **Mechanic (Mechanik)**
-  **Cashier (Kasjer)**
-  **Driver (Kierowca)**





