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
    init_ipc(0);

    srand(getpid() ^ time(NULL));

    Msg msg;
    msg.mtype = MSG_REJESTRACJA;
    msg.samochod.pid_kierowcy = getpid();
    msg.samochod.zaakceptowano = 0;
    msg.samochod.dodatkowa_usterka = 0;

    //Losowanie marki samochodu
    char marka = 'A' + rand() % 26;
    msg.samochod.marka[0] = marka;
    msg.samochod.marka[1] = '\0';

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
            int czas_do_otwarcia = GODZINA_OTWARCIA - godzina;
            if (czas_do_otwarcia < 0)
            {
                czas_do_otwarcia += 24;
            }

            if (u.krytyczna || czas_do_otwarcia <= LIMIT_OCZEKIWANIA)
            {
                printf("[KIEROWCA %d] Serwis zamknięty, ale czekam na otwarcie...\n", getpid());
                sleep(2);
                continue;
            }
            else
            {
                printf("[KIEROWCA %d] Serwis zamknięty, odjeżdżam\n", getpid());
                return 0;
            }
        }
    }

    sem_lock(SEM_SHARED);
    shared->liczba_oczekujacych_klientow++;
    printf("[KIEROWCA %d] Dołączam do kolejki. Liczba oczekujących klientów: %d\n", getpid(), shared->liczba_oczekujacych_klientow);
    sem_unlock(SEM_SHARED);

    //Wysłanie do rejestracji
    if (msgsnd(msg_id, &msg, sizeof(Samochod), 0) == -1)
    {
        perror("[KIEROWCA %d] Błąd rejestracji");
        return 1;
    }
    printf("[KIEROWCA %d] Samochód wysłany do rejestracji\n", getpid());

    //Odbiór wyceny
    if (msgrcv(msg_id, &msg, sizeof(Samochod), getpid(), 0) == -1)
    {
        perror("[KIEROWCA] Błąd odbioru wyceny");
        return 1;
    }
    printf("[KIEROWCA %d] Otrzymana wycena: %d PLN, %d s\n", getpid(), msg.samochod.koszt, msg.samochod.czas_naprawy);

    //Decyzja
    int rezygnacja = (rand() % 100) < 2; 

    msg.mtype = MSG_DECYZJA_USLUGI;
    msg.samochod.zaakceptowano = !rezygnacja;

    if (msgsnd(msg_id, &msg, sizeof(Samochod), 0) == -1)
    {
        perror("[KIEROWCA] Błąd wysłania decyzji");
        return 1;
    }

    if (rezygnacja)
    {
        printf("[KIEROWCA %d] Rezygnuję z naprawy, odjeżdżam\n", getpid());
        return 0;
    }

    printf("[KIEROWCA %d] Akceptuję wycenę. Czekam na naprawę...\n", getpid());

    while (1)
    {
        if (msgrcv(msg_id, &msg, sizeof(Samochod), getpid(), 0) == -1)
        {
            perror("[KIEROWCA] Błąd odbioru wiadomości");
            break;
        }

        if (msg.samochod.dodatkowa_usterka > 0)
        {
            Usluga dodatkowa = pobierz_usluge(msg.samochod.id_dodatkowej_uslugi); 

            printf("[KIEROWCA %d] Pracownik Serwisu zgłosił dodatkową usterkę! %s, +%d PLN, +%d s\n", getpid(), dodatkowa.nazwa, msg.samochod.dodatkowy_koszt, msg.samochod.dodatkowy_czas);

            int odmowa = (rand() % 100) < 20;

            msg.mtype = MSG_DECYZJA_DODATKOWA;
            msg.samochod.zaakceptowano = !odmowa;

            msgsnd(msg_id, &msg, sizeof(Samochod), 0);
            printf("[KIEROWCA %d] Decyzja w sprawie dodatkowej usterki: %s\n", getpid(), odmowa ? "Odrzucam" : "Akceptuję");

            continue;
        }
        else
        {
            printf("[KIEROWCA %d] Zapłacono w kasie %d PLN. Odbieram kluczyki i odjeżdżam\n", getpid(), msg.samochod.koszt);
            break;
        }
    }
    return 0;
}