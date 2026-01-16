#define _DEFAULT_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/msg.h>
#include <errno.h>

int main()
{
    //Dołączenie do IPC
    init_ipc(0);

    srand(getpid() ^ time(NULL));

    Msg msg;
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

    printf("[KIEROWCA %d] Marka samochodu: %s\n", getpid(), msg.samochod.marka);
    printf("[KIEROWCA %d] Potrzebna naprawa: %s\n", getpid(), u.nazwa);

    //Sprawdzenie czy marka jest obsługiwana
    if (!marka_obslugiwana(msg.samochod.marka))
    {
        printf("[KIEROWCA %d] Marka nieobsługiwana, odjeżdżam\n", getpid());
        return 0;
    }

    //Czekanie na otwarcie serwisu
    while(1)
    {
        sem_lock(SEM_SHARED);
        int otwarte = shared->serwis_otwarty;
        int godzina = shared->aktualna_godzina;
        sem_unlock(SEM_SHARED);

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
                wait_serwis_otwarty();
                continue;
            }
            else
            {
                printf("[KIEROWCA %d] Serwis zamknięty, odjeżdżam\n", getpid());
                return 0;
            }
        }
    }

    //Dołączenie do kolejki oczekujących klientów
    sem_lock(SEM_SHARED);
    shared->liczba_oczekujacych_klientow++;
    printf("[KIEROWCA %d] Dołączam do kolejki. Liczba oczekujących klientów: %d\n", getpid(), shared->liczba_oczekujacych_klientow);
    sem_unlock(SEM_SHARED);

    //Wysłanie do rejestracji
    if (send_msg(msg_id, &msg) == -1)
    {
        perror("[KIEROWCA] Błąd rejestracji");
        return 1;
    }
    signal_nowa_wiadomosc();
    printf("[KIEROWCA %d] Samochód wysłany do rejestracji\n", getpid());

    //Odbiór wyceny
    while (1)
    {
        sem_lock(SEM_SHARED);
        if (shared->pozar)
        {
            sem_unlock(SEM_SHARED);
            printf("[KIEROWCA %d] Pożar! Uciekam z serwisu!\n", getpid());
            return 0;
        }
        sem_unlock(SEM_SHARED);

        if (recv_msg(msg_id, &msg, getpid(), IPC_NOWAIT) != -1)
        {
            //Otrzymano wycenę
            break;
        }

        if (errno != ENOMSG)
        {
            perror("[KIEROWCA] Błąd odbioru wyceny");
            return 1;
        }

        wait_nowa_wiadomosc(0);
    }

    //Sprawdzenie czy serwis może wykonać naprawę (z powodu zamknięcia lub pożaru)
    if (msg.samochod.koszt == 0 && !msg.samochod.ewakuacja)
    {
        printf("[KIEROWCA %d] Serwis nie może wykonać naprawy, odjeżdżam\n", getpid());
        return 0;
    }

    printf("[KIEROWCA %d] Otrzymana wycena: %d PLN, %d s\n", getpid(), msg.samochod.koszt, msg.samochod.czas_naprawy);

    //Decyzja kierowcy
    //2% szans na rezygnację
    int rezygnacja = (rand() % 100) < 2;

    msg.mtype = MSG_DECYZJA_USLUGI(msg.samochod.id_pracownika);
    msg.samochod.zaakceptowano = !rezygnacja;

    if (send_msg(msg_id, &msg) == -1)
    {
        perror("[KIEROWCA] Błąd wysłania decyzji");
        return 1;
    }
    signal_nowa_wiadomosc();

    if (rezygnacja)
    {
        printf("[KIEROWCA %d] Rezygnuję z naprawy, odjeżdżam\n", getpid());
        return 0;
    }

    printf("[KIEROWCA %d] Akceptuję wycenę. Czekam na naprawę...\n", getpid());

    //Oczekiwanie na zakończenie naprawy
    while (1)
    {
        sem_lock(SEM_SHARED);
        int pozar = shared->pozar;
        sem_unlock(SEM_SHARED);

        if (pozar)
        {
            printf("[KIEROWCA %d] Widzę ogień! Biorę kluczyki z lady i uciekam\n", getpid());
            break;
        }

        //Odbiór wiadomości zwrotnych
        if (recv_msg(msg_id, &msg, getpid(), IPC_NOWAIT) == -1)
        {
            if (errno == ENOMSG)
            {
                wait_nowa_wiadomosc(0);
                continue;
            }
            perror("[KIEROWCA] Błąd odbioru wiadomości");
            break;
        }

        //Obsluga sytuacji wyjątkowych
        if (msg.samochod.ewakuacja)
        {
            printf("[KIEROWCA %d] Mechanik oddał kluczyki z powodu pożaru! Uciekam!\n", getpid());
            break;
        }

        //Obsługa dodatkowych usterek
        if (msg.samochod.dodatkowa_usterka > 0)
        {
            Usluga dodatkowa = pobierz_usluge(msg.samochod.id_dodatkowej_uslugi); 

            printf("[KIEROWCA %d] Pracownik Serwisu zgłosił dodatkową usterkę! %s, +%d PLN, +%d s\n", getpid(), dodatkowa.nazwa, msg.samochod.dodatkowy_koszt, msg.samochod.dodatkowy_czas);

            //20% szans na odrzucenie dodatkowej usterki
            int odmowa = (rand() % 100) < 20;

            msg.mtype = MSG_DECYZJA_DODATKOWA(msg.samochod.id_pracownika);
            msg.samochod.zaakceptowano = !odmowa;

            send_msg(msg_id, &msg);
            printf("[KIEROWCA %d] Decyzja w sprawie dodatkowej usterki: %s\n", getpid(), odmowa ? "Odrzucam" : "Akceptuję");

            //Czekanie na kontynuację naprawy
            continue;
        }
        else
        {
            //Koniec naprawy
            printf("[KIEROWCA %d] Zapłacono w kasie %d PLN. Odbieram kluczyki i odjeżdżam\n", getpid(), msg.samochod.koszt);
            break;
        }
    }
    return 0;
}