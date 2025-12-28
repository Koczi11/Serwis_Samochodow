#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/msg.h>

int main()
{
    srand(getpid());

    Msg msg;
    msg.mtype = MSG_REJESTRACJA;
    msg.samochod.pid_kierowcy = getpid();

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
    msgsnd(msg_id, &msg, sizeof(Samochod), 0);
    printf("[KIEROWCA %d] Samochód wysłany do rejestracji\n", getpid());

    //Odbiór wyceny
    msgrcv(msg_id, &msg, sizeof(Samochod), getpid(), 0);
    printf("[KIEROWCA %d] Otrzymana wycena: %d PLN, %d s\n", getpid(), msg.samochod.koszt, msg.samochod.czas_naprawy);

    //Decyzja
    msg.mtype = MSG_DECYZJA;
    msg.samochod.zaakceptowano = rand() % 2;
    msgsnd(msg_id, &msg, sizeof(Samochod), 0);

    printf("[KIEROWCA %d] Decyzja: %s\n", getpid(), msg.samochod.zaakceptowano ? "Akceptuję" : "Odrzucam");

    //Dodatkowa usterka
    if (msgrcv(msg_id, &msg, sizeof(Samochod), MSG_PYTANIE, IPC_NOWAIT) != -1)
    {
        printf("[KIEROWCA %d] Pytanie o dodatkową usterkę: +%d PLN, +%d s\n", getpid(), msg.samochod.dodatkowy_koszt, msg.samochod.dodatkowy_czas);

        msg.samochod.zaakceptowano = rand() % 2;
        msg.mtype = MSG_ODPOWIEDZ;
        msgsnd(msg_id, &msg, sizeof(Samochod), 0);

        printf("[KIEROWCA %d] Decyzja dodatkowej usterki: %s\n", getpid(), msg.samochod.zaakceptowano ? "Akceptuję" : "Odrzucam");
    }

    return 0;
}