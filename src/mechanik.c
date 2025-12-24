#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int znajdz_stanowisko()
{
    for (int i = 0; i < MAX_STANOWISK; i++)
    {
        if (shared->stanowiska[i].zajete == 0)
        {
            return i;
        }
    }
    return -1;
}

int main()
{
    Msg msg;

    printf("[MECHANIK %d] Gotowy do pracy\n", getpid());

    while (1)
    {
        //Odbiór samochodu do naprawy
        msgrcv(msg_id, &msg, sizeof(Samochod), MSG_NAPRAWA, 0);

        //Znajdź wolne stanowisko
        sem_lock(SEM_STANOWISKA);
        sem_lock(SEM_SHARED);

        int s = znajdz_stanowisko();
        if (s == -1)
        {
            sem_unlock(SEM_SHARED);
            sem_unlock(SEM_STANOWISKA);
            continue;
        }

        shared->stanowiska[s].zajete = 1;
        shared->stanowiska[s].pid_mechanika = getpid();

        sem_unlock(SEM_SHARED);
        sem_unlock(SEM_STANOWISKA);

        printf("[MECHANIK %d] Naprawa auta %d na stanowisku %d\n", getpid(), msg.samochod.pid_kierowcy, s);

        sleep(msg.samochod.czas_naprawy); 

        //Zwolnienie stanowiska
        sem_lock(SEM_SHARED);
        shared->stanowiska[s].zajete = 0;
        shared->stanowiska[s].pid_mechanika = -1;
        sem_unlock(SEM_SHARED);

        printf("[MECHANIK %d] Zakończono naprawę auta %d\n", getpid(), msg.samochod.pid_kierowcy);

        //Informacja o zakończeniu naprawy
        msg.mtype = MSG_KONIEC_NAPRAWY;
        msgsnd(msg_id, &msg, sizeof(Samochod), 0);
    }
}