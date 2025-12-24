#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>

int main()
{
    Msg msg;

    printf("[PRACOWNIK SERWISU] Rejestracja uruchomiona\n");

    while (1)
    {
        //Odbiór klienta 
        msgrcv(msg_id, &msg, sizeof(Samochod), MSG_REJESTRACJA, 0);

        printf("[PRACOWNIK SERWISU] Obsługa kierowcy %d, marka %s\n", msg.samochod.pid_kierowcy, msg.samochod.marka);

        //Wycena naprawy
        msg.samochod.czas_naprawy = 5 + rand() % 20;
        msg.samochod.koszt = 200 + rand() % 500;

        //Odesłanie wyceny
        msg.mtype = msg.samochod.pid_kierowcy;
        msgsnd(msg_id, &msg, sizeof(Samochod), 0);

        //Odbiór decyzji
        msgrcv(msg_id, &msg, sizeof(Samochod), MSG_DECYZJA, 0);

        if (msg.samochod.zaakceptowano)
        {
            printf("[PRACOWNIK SERWISU] Kierowca %d zaakceptował usługę\n", msg.samochod.pid_kierowcy);
            
            msg.mtype = MSG_NAPRAWA;
            msgsnd(msg_id, &msg, sizeof(Samochod), 0);
        }
        else
        {
            printf("[PRACOWNIK SERWISU] Kierowca %d odrzucił usługę\n", msg.samochod.pid_kierowcy);
        }
    }
}