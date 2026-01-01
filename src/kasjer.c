#include "serwis_ipc.h"
#include <stdio.h>
#include <unistd.h>

int main()
{
    init_ipc(0);

    Msg msg;
    char buffer[256];

    printf("[KASJER] Kasa otwarta\n");

    while (1)
    {
        //Czekamy na klienta do zapłaty
        if(msgrcv(msg_id, &msg, sizeof(Samochod), MSG_KASA, 0) == -1)
        {
            perror("[KASJER] Błąd odbioru wiadomości");
            continue;
        }

        printf("[KASJER] Klient %d płaci %d PLN\n", msg.samochod.pid_kierowcy, msg.samochod.koszt);
        sleep(2); //Symulacja płatności

        snprintf(buffer, sizeof(buffer), "[KASJER] Pobrano opłatę %d PLN od kierowcy %d", msg.samochod.koszt, msg.samochod.pid_kierowcy);
        zapisz_raport(buffer);

        msg.mtype = MSG_ZAPLATA;
        
        if(msgsnd(msg_id, &msg, sizeof(Samochod), 0) == -1)
        {
            perror("[KASJER] Błąd wysłania wiadomości");
            continue;
        }

        printf("[KASJER] Płatność zakończona dla klienta %d\n", msg.samochod.pid_kierowcy);
    }
    return 0;
}