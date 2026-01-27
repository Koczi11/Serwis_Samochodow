#define _DEFAULT_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/msg.h>
#include <errno.h>

//Flaga ewakuacji
volatile sig_atomic_t ewakuacja = 0;

//Obsługa sygnału pożaru
void handle_pozar(int sig)
{
    (void)sig;
    ewakuacja = 1;
}

int main()
{
    //Dołączenie do IPC
    init_ipc(0);
    join_service_group();

    //Rejestracja handlera pożaru
    struct sigaction sa;
    sa.sa_handler = handle_pozar;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1)
    {
        perror("sigaction SIGUSR1 failed");
    }

    srand(getpid() ^ time(NULL));

    Msg msg;
    char buffer[256];
    msg.mtype = MSG_REJESTRACJA;
    msg.samochod.pid_kierowcy = getpid();
    msg.samochod.zaakceptowano = 0;
    msg.samochod.dodatkowa_usterka = 0;
    msg.samochod.ewakuacja = 0;
    msg.samochod.id_pracownika = -1;

    //Losowanie marki samochodu
    char marka = 'A' + rand() % 26;
    msg.samochod.marka[0] = marka;
    msg.samochod.marka[1] = '\0';

    //Losowanie usługi
    int wybrana_usluga = rand() % MAX_USLUG;
    msg.samochod.id_uslugi = wybrana_usluga;

    Usluga u = pobierz_usluge(wybrana_usluga);

    printf("[KIEROWCA %d] Marka samochodu: %s. Potrzebna naprawa: %s\n", getpid(), msg.samochod.marka, u.nazwa);
    snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Marka samochodu: %s. Potrzebna naprawa: %s", getpid(), msg.samochod.marka, u.nazwa);
    zapisz_log(buffer);

    //Czekanie na otwarcie serwisu
    while(1)
    {
        if (ewakuacja)
        {
            printf("[KIEROWCA %d] Otrzymano sygnał pożaru! Uciekam!\n", getpid());
            snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Otrzymano sygnał pożaru! Uciekam!", getpid());
            zapisz_log(buffer);
            return 0;
        }

        sem_lock(SEM_STATUS);
        int otwarte = shared->serwis_otwarty;
        int godzina = shared->aktualna_godzina;
        sem_unlock(SEM_STATUS);

        if (otwarte)
        {
            break;
        }
        else
        {
            //Obliczanie czasu do otwarcia
            int czas_do_otwarcia = GODZINA_OTWARCIA - godzina;
            if (czas_do_otwarcia < 0)
            {
                czas_do_otwarcia += 24;
            }

            //Kierowca czeka tylko jeśli usługa jest krytyczna lub czas oczekiwania jest krótki
            if (u.krytyczna || czas_do_otwarcia <= LIMIT_OCZEKIWANIA)
            {
                printf("[KIEROWCA %d] Serwis zamknięty, ale czekam na otwarcie...\n", getpid());
                snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Serwis zamknięty, ale czekam na otwarcie...", getpid());
                zapisz_log(buffer);

                sem_lock(SEM_LICZNIKI);
                shared->liczba_czekajacych_na_otwarcie++;
                sem_unlock(SEM_LICZNIKI);

                while (1)
                {
                    if (ewakuacja)
                    {
                        printf("[KIEROWCA %d] Otrzymano sygnał pożaru! Uciekam!\n", getpid());
                        snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Otrzymano sygnał pożaru! Uciekam!", getpid());
                        zapisz_log(buffer);
                        return 0;
                    }

                    sem_lock(SEM_STATUS);
                    int otwarte_po = shared->serwis_otwarty;
                    sem_unlock(SEM_STATUS);

                    if (otwarte_po)
                    {
                        break;
                    }

                    safe_wait_seconds(0.2);
                }
                continue;
            }
            else
            {
                printf("[KIEROWCA %d] Serwis zamknięty, odjeżdżam\n", getpid());
                snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Serwis zamknięty, odjeżdżam", getpid());
                zapisz_log(buffer);

                return 0;
            }
        }
    }

    //Dołączenie do kolejki oczekujących klientów
    sem_lock(SEM_LICZNIKI);
    shared->liczba_oczekujacych_klientow++;
    sem_unlock(SEM_LICZNIKI);

    printf("[KIEROWCA %d] Dołączam do kolejki. Liczba oczekujących klientów: %d\n", getpid(), shared->liczba_oczekujacych_klientow);
    snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Dołączam do kolejki. Liczba oczekujących klientów: %d", getpid(), shared->liczba_oczekujacych_klientow);
    zapisz_log(buffer);

    //Wysłanie do rejestracji
    int s = send_msg(msg_id_kierowca, &msg);
    if (s == -2)
    {
        return 0;
    }
    if (s == -1)
    {
        perror("[KIEROWCA] Błąd rejestracji");
        return 1;
    }

    printf("[KIEROWCA %d] Samochód wysłany do rejestracji\n", getpid());
    snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Samochód wysłany do rejestracji", getpid());
    zapisz_log(buffer);

    //Odbiór wyceny
    while (1)
    {
        if (ewakuacja)
        {
            printf("[KIEROWCA %d] Otrzymano sygnał pożaru! Uciekam!\n", getpid());
            snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Otrzymano sygnał pożaru! Uciekam!", getpid());
            zapisz_log(buffer);
            return 0;
        }

        int r = recv_msg(msg_id_kierowca, &msg, MSG_KIEROWCA(getpid()), 0);
        if (r == -2)
        {
            return 0;
        }
        if (r != -1)
        {
            //Otrzymano wycenę
            break;
        }

        if (errno != EINTR)
        {
            perror("[KIEROWCA] Błąd odbioru wyceny");
            return 1;
        }
    }

    if (msg.samochod.ewakuacja)
    {
        printf("[KIEROWCA %d] Ewakuacja podczas oczekiwania na wycenę! Uciekam!\n", getpid());
        snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Ewakuacja podczas oczekiwania na wycenę! Uciekam!", getpid());
        zapisz_log(buffer);

        return 0;
    }

    if (msg.samochod.koszt < 0)
    {
        printf("[KIEROWCA %d] Marka nieobsługiwana przez serwis, odjeżdżam\n", getpid());
        snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Marka nieobsługiwana przez serwis, odjeżdżam", getpid());
        zapisz_log(buffer);

        return 0;
    }

    printf("[KIEROWCA %d] Otrzymana wycena: %d PLN, %d s\n", getpid(), msg.samochod.koszt, msg.samochod.czas_naprawy);
    snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Otrzymana wycena: %d PLN, %d s", getpid(), msg.samochod.koszt, msg.samochod.czas_naprawy);
    zapisz_log(buffer);

    //Decyzja kierowcy
    //2% szans na rezygnację
    int rezygnacja = (rand() % 100) < 2;

    msg.mtype = MSG_DECYZJA_USLUGI_PID(getpid());
    msg.samochod.zaakceptowano = !rezygnacja;

    s = send_msg(msg_id_kierowca, &msg);
    if (s == -2)
    {
        return 0;
    }
    if (s == -1)
    {
        perror("[KIEROWCA] Błąd wysłania decyzji");
        return 1;
    }

    if (rezygnacja)
    {
        printf("[KIEROWCA %d] Rezygnuję z naprawy, odjeżdżam\n", getpid());
        snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Rezygnuję z naprawy, odjeżdżam", getpid());
        zapisz_log(buffer);
        return 0;
    }

    printf("[KIEROWCA %d] Akceptuję wycenę. Czekam na naprawę...\n", getpid());
    snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Akceptuję wycenę. Czekam na naprawę...", getpid());
    zapisz_log(buffer);

    //Oczekiwanie na zakończenie naprawy
    while (1)
    {
        if (ewakuacja)
        {
            printf("[KIEROWCA %d] Otrzymano sygnał pożaru! Uciekam!\n", getpid());
            snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Otrzymano sygnał pożaru! Uciekam!", getpid());
            zapisz_log(buffer);
            break;
        }

        //Odbiór wiadomości zwrotnych
        int r = recv_msg(msg_id_kierowca, &msg, MSG_KIEROWCA(getpid()), 0);
        if (r == -2)
        {
            return 0;
        }
        if (r == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("[KIEROWCA] Błąd odbioru wiadomości");
            break;
        }

        //Obsluga sytuacji wyjątkowych
        if (msg.samochod.ewakuacja)
        {
            printf("[KIEROWCA %d] Mechanik oddał kluczyki z powodu pożaru! Uciekam!\n", getpid());
            snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Mechanik oddał kluczyki z powodu pożaru! Uciekam!", getpid());
            zapisz_log(buffer);

            return 0;
        }

        //Obsługa dodatkowych usterek
        if (msg.samochod.dodatkowa_usterka > 0)
        {
            Usluga dodatkowa = pobierz_usluge(msg.samochod.id_dodatkowej_uslugi); 

            printf("[KIEROWCA %d] Pracownik Serwisu zgłosił dodatkową usterkę! %s, +%d PLN, +%d s\n", getpid(), dodatkowa.nazwa, msg.samochod.dodatkowy_koszt, msg.samochod.dodatkowy_czas);
            snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Pracownik Serwisu zgłosił dodatkową usterkę! %s, +%d PLN, +%d s", getpid(), dodatkowa.nazwa, msg.samochod.dodatkowy_koszt, msg.samochod.dodatkowy_czas);
            zapisz_log(buffer);

            //20% szans na odrzucenie dodatkowej usterki
            int odmowa = (rand() % 100) < 20;

            msg.mtype = MSG_DECYZJA_DODATKOWA_PID(getpid());
            msg.samochod.zaakceptowano = !odmowa;

            s = send_msg(msg_id_kierowca, &msg);
            if (s == -2)
            {
                return 0;
            }
            printf("[KIEROWCA %d] Decyzja w sprawie dodatkowej usterki: %s\n", getpid(), odmowa ? "Odrzucam" : "Akceptuję");
            snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Decyzja w sprawie dodatkowej usterki: %s", getpid(), odmowa ? "Odrzucam" : "Akceptuję");
            zapisz_log(buffer);

            //Czekanie na kontynuację naprawy
            continue;
        }
        else
        {
            //Koniec naprawy
            printf("[KIEROWCA %d] Zapłacono w kasie %d PLN. Odbieram kluczyki i odjeżdżam\n", getpid(), msg.samochod.koszt);
            snprintf(buffer, sizeof(buffer), "[KIEROWCA %d] Zapłacono w kasie %d PLN. Odbieram kluczyki i odjeżdżam", getpid(), msg.samochod.koszt);
            zapisz_log(buffer);
            
            break;
        }
    }
    return 0;
}