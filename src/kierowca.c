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

    //Losowanie marki samochodu
    char marka = 'A' + rand() % 26;
    msg.samochod.marka[0] = marka;
    msg.samochod.marka[1] = '\0';

    printf("[KIEROWCA %d] Marka samochodu: %s\n", getpid(), msg.samochod.marka);

    //Sprawdzenie czy marka jest obsługiwana
    if (!marka_obslugiwana(msg.samochod.marka))
    {
        printf("[KIEROWCA %d] Marka nieobsługiwana, odjeżdżam\n", getpid());
        return 0;
    }

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

        if (msg.samochod.dodatkowa_usterka > 0 && msg.samochod.zaakceptowano == 0)
        {
            printf("[KIEROWCA %d] Dodatkowa usterka! +%d PLN, +%d s\n", getpid(), msg.samochod.dodatkowy_koszt, msg.samochod.dodatkowy_czas);

            int odmowa = (rand() % 100) < 20;

            msg.mtype = MSG_ODPOWIEDZ;
            msg.samochod.zaakceptowano = !odmowa;

            msgsnd(msg_id, &msg, sizeof(Samochod), 0);
            printf("[KIEROWCA %d] Decyzja w sprawie dodatkowej usterki: %s\n", getpid(), odmowa ? "Odrzucam" : "Akceptuję");

            continue;
        }
        else
        {
            printf("[KIEROWCA %d] Samochód gotowy. Do zapłaty: %d PLN\n", getpid(), msg.samochod.koszt);
            break;
        }
    }

    printf("[KIEROWCA %d] Odjeżdżam z serwisu. Dziękuję!\n", getpid());
    return 0;
}