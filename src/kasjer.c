#define _GNU_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/msg.h>

int aktywni_mechanicy()
{
    int aktywni = 0;
    sem_lock(SEM_SHARED);
    for (int i = 0; i < MAX_STANOWISK; i++)
    {
        if (shared->stanowiska[i].zajete)
        {
            aktywni = 1;
            break;
        }
    }
    sem_unlock(SEM_SHARED);
    return aktywni;
}

int main()
{
    init_ipc(0);

    Msg msg;
    char buffer[256];

    while (1)
    {
        while (1)
        {
            sem_lock(SEM_SHARED);
            if (shared->serwis_otwarty && !shared->pozar)
            {
                sem_unlock(SEM_SHARED);
                break;
            }
            sem_unlock(SEM_SHARED);
            sleep(1);
        }
    
        printf("[KASJER] Kasa otwarta\n");

        while (1)
        {
            sem_lock(SEM_SHARED);
            int pozar = shared->pozar;
            int otwarte = shared->serwis_otwarty;
            sem_unlock(SEM_SHARED);

            if (pozar)
            {
                printf("[KASJER] Pożar!\n");
                break;
            }

            //Czekamy na klienta do zapłaty
            if(msgrcv(msg_id, &msg, sizeof(Samochod), MSG_KASA, IPC_NOWAIT) != -1)
            {
                printf("[KASJER] Klient %d płaci %d PLN\n", msg.samochod.pid_kierowcy, msg.samochod.koszt);
                sleep(2); //Symulacja płatności

                snprintf(buffer, sizeof(buffer), "[KASJER] Pobrano opłatę %d PLN od kierowcy %d", msg.samochod.koszt, msg.samochod.pid_kierowcy);
                zapisz_raport(buffer);

                msg.mtype = msg.samochod.pid_kierowcy;
                msg.samochod.dodatkowa_usterka = 0;
                
                if(msgsnd(msg_id, &msg, sizeof(Samochod), 0) == -1)
                {
                    perror("[KASJER] Błąd wysłania wiadomości");
                }
                else
                {
                    printf("[KASJER] Płatność zakończona dla klienta %d\n", msg.samochod.pid_kierowcy);

                    sem_lock(SEM_SHARED);
                    if (shared->auta_w_serwisie > 0)
                    {
                        shared->auta_w_serwisie--;
                    }
                    sem_unlock(SEM_SHARED);
                }

                continue;
            }

            if (!otwarte)
            {
                int auta_zostaly = 0;
                sem_lock(SEM_SHARED);
                auta_zostaly = shared->auta_w_serwisie;
                sem_unlock(SEM_SHARED);

                if (!aktywni_mechanicy() && auta_zostaly == 0)
                {
                    if (msgrcv(msg_id, &msg, sizeof(Samochod), MSG_KASA, IPC_NOWAIT) == -1)
                    {
                        printf("[KASJER] Kasa zamknięta, brak klientów\n");
                        break;
                    }
                }
            }

            usleep(100000);
        }
    }
    return 0;
}