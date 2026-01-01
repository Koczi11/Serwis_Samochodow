#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>

int sprawdz_dlugosc_kolejki()
{
    struct msqid_ds buf;
    if (msgctl(msg_id, IPC_STAT, &buf) == -1)
    {
        return 0;
    }
    return buf.msg_qnum;
}

int znajdz_wolne_stanowisko(const char *marka)
{
    int is_UY = (strcmp(marka, "U") == 0 || strcmp(marka, "Y") == 0);

    for (int i = 0; i < MAX_STANOWISK; i++)
    {
        if (i == 7 && !is_UY)
            continue;

        sem_lock(SEM_SHARED);
        int zajete = shared->stanowiska[i].zajete;
        sem_unlock(SEM_SHARED);

        if (!zajete)
            return i;
    }
    return -1;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Brak ID pracownika serwisu\n");
        exit(1);
    }

    int id_pracownika = atoi(argv[1]);
    init_ipc(0);

    Msg msg;

    printf("[PRACOWNIK SERWISU %d] Uruchomiony\n", id_pracownika);

    while (1)
    {
        int q_len = sprawdz_dlugosc_kolejki();

        if (id_pracownika == 1 && q_len <=3)
        {
            sleep(1);
            continue;
        }

        if (id_pracownika == 2 && q_len <= 5)
        {
            sleep(1);
            continue;
        }

        //Odbiór klienta 
        if (msgrcv(msg_id, &msg, sizeof(Samochod), MSG_REJESTRACJA, 0) == -1)
        {
            perror("[PRACOWNIK SERWISU] Błąd odbioru rejestracji");
            exit(1);
        }

        printf("[PRACOWNIK SERWISU %d] Obsługa kierowcy %d, marka %s\n", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.marka);

        //Wycena naprawy
        msg.samochod.czas_naprawy = 5 + rand() % 20;
        msg.samochod.koszt = 200 + rand() % 500;

        msg.samochod.zaakceptowano = 0;
        msg.samochod.dodatkowa_usterka = 0;

        //Odesłanie wyceny
        msg.mtype = msg.samochod.pid_kierowcy;
        msgsnd(msg_id, &msg, sizeof(Samochod), 0);

        Msg decyzja;

        while (1)
        {
            if (msgrcv(msg_id, &decyzja, sizeof(Samochod), MSG_DECYZJA_USLUGI, 0) == -1)
            {
                break;
            }

            if (decyzja.samochod.pid_kierowcy == msg.samochod.pid_kierowcy)
            {
                msg = decyzja;
                break;
            }
            else
            {
                msgsnd(msg_id, &decyzja, sizeof(Samochod), 0);
            }
        }

        if (msg.samochod.zaakceptowano)
        {
            printf("[PRACOWNIK SERWISU %d] Kierowca %d zaakceptował usługę\n", id_pracownika, msg.samochod.pid_kierowcy);
            
            int mechanik_id = -1;

            while (mechanik_id == -1)
            {
                mechanik_id = znajdz_wolne_stanowisko(msg.samochod.marka);
                if (mechanik_id == -1)
                {
                    printf("[PRACOWNIK SERWISU %d] Brak wolnych mechaników dla %s, czekam...\n", id_pracownika, msg.samochod.marka);
                    sleep(2);
                }
            }

            msg.mtype = 100 + mechanik_id;
            msgsnd(msg_id, &msg, sizeof(Samochod), 0);
            printf("[PRACOWNIK SERWISU %d] Przekazano auto %d do mechanika %d\n", id_pracownika, msg.samochod.pid_kierowcy, mechanik_id);
        }
        else
        {
            printf("[PRACOWNIK SERWISU %d] Kierowca %d odrzucił usługę\n", id_pracownika, msg.samochod.pid_kierowcy);
        }
    }
    return 0;
}