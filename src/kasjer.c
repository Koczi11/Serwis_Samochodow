#include "serwis_ipc.h"
#include <stdio.h>
#include <unistd.h>

int main()
{
    Msg msg;

    printf("[KASJER] Kasa otwarta\n");

    while (1)
    {
        //Czekamy na klienta do zapłaty
        msgrcv(msg_id, &msg, sizeof(Samochod), MSG_KASA, 0);

        printf("[KASJER] Klient %d płaci %d PLN\n", msg.samochod.pid_kierowcy, msg.samochod.koszt);
        sleep(2); //Symulacja płatności

        msg.mtype = MSG_ZAPLATA;
        msgsnd(msg_id, &msg, sizeof(Samochod), 0);

        printf("[KASJER] Płatność zakończona dla klienta %d\n", msg.samochod.pid_kierowcy);
    }
}