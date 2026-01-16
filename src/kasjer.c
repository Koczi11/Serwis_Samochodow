#define _GNU_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/msg.h>

//Funkcja sprawdzająca, czy są aktywni mechanicy
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
    //Dołączanie do IPC
    init_ipc(0);

    Msg msg;
    //Bufor do raportów
    char buffer[256];

    //Pętla dni pracy
    while (1)
    {
        wait_serwis_otwarty();

        printf("[KASJER] Kasa otwarta\n");
        snprintf(buffer, sizeof(buffer), "[KASJER] Kasa otwarta");
        zapisz_log(buffer);

        int dzienny_utarg = 0;
        zapisz_raport("[KASJER] Kasa otwarta. Rozpoczynamy nowy dzień.");

        //Pętla obsługi klientów
        while (1)
        {
            sem_lock(SEM_SHARED);
            int pozar = shared->pozar;
            int otwarte = shared->serwis_otwarty;
            sem_unlock(SEM_SHARED);

            if (pozar)
            {
                printf("[KASJER] Pożar!\n");
                snprintf(buffer, sizeof(buffer), "[KASJER] Pożar!");
                zapisz_log(buffer);

                break;
            }

            //Obsługa płatności klientów
            if(recv_msg(msg_id, &msg, MSG_KASA, IPC_NOWAIT) != -1)
            {
                printf("[KASJER] Klient %d płaci %d PLN\n", msg.samochod.pid_kierowcy, msg.samochod.koszt);
                snprintf(buffer, sizeof(buffer), "[KASJER] Klient %d płaci %d PLN", msg.samochod.pid_kierowcy, msg.samochod.koszt);
                zapisz_log(buffer);
                safe_wait_seconds(2);

                //Aktualizacja dziennego utargu
                dzienny_utarg += msg.samochod.koszt;

                //Zapisujemy raport o płatności
                snprintf(buffer, sizeof(buffer), "[KASJER] Pobrano opłatę %d PLN od kierowcy %d", msg.samochod.koszt, msg.samochod.pid_kierowcy);
                zapisz_raport(buffer);

                //Odsyłamy potwierdzenie płatności do kierowcy
                msg.mtype = msg.samochod.pid_kierowcy;
                msg.samochod.dodatkowa_usterka = 0;
                
                if(send_msg(msg_id, &msg) == -1)
                {
                    perror("[KASJER] Błąd wysłania wiadomości");
                }
                else
                {
                    printf("[KASJER] Płatność zakończona dla klienta %d\n", msg.samochod.pid_kierowcy);
                    snprintf(buffer, sizeof(buffer), "[KASJER] Płatność zakończona dla klienta %d", msg.samochod.pid_kierowcy);
                    zapisz_log(buffer);

                    //Dekrementacja liczniy aut w serwisie
                    sem_lock(SEM_SHARED);
                    if (shared->auta_w_serwisie > 0)
                    {
                        shared->auta_w_serwisie--;
                    }
                    sem_unlock(SEM_SHARED);
                }

                //Przejdź do obsługi następnego klienta
                continue;
            }

            //Procedura zamknięcia kasy, jeśli serwis jest zamknięty i nie ma klientów
            if (!otwarte)
            {
                int auta_zostaly = 0;
                sem_lock(SEM_SHARED);
                auta_zostaly = shared->auta_w_serwisie;
                sem_unlock(SEM_SHARED);

                if (!aktywni_mechanicy() && auta_zostaly == 0)
                {
                    //Ostanie sprawdzenie wiadomości
                    if (recv_msg(msg_id, &msg, MSG_KASA, IPC_NOWAIT) == -1)
                    {
                        printf("[KASJER] Kasa zamknięta, brak klientów\n");
                        snprintf(buffer, sizeof(buffer), "[KASJER] Kasa zamknięta o godzinie %d, brak klientów", shared->aktualna_godzina);
                        zapisz_log(buffer);
                        
                        break;
                    }
                }
            }

            wait_nowa_wiadomosc(0);
        }

        sem_lock(SEM_SHARED);
        int pozar = shared->pozar;
        sem_unlock(SEM_SHARED);

        //Zapisujemy raport o zamknięciu kasy
        if (pozar)
        {
            snprintf(buffer, sizeof(buffer), "[KASJER] Dzień przerwany przez pożar. Zebrany utarg do momentu ewakuacji: %d PLN", dzienny_utarg);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "[KASJER] Kasa zamknięta. Dzienny utarg: %d PLN", dzienny_utarg);
        }

        zapisz_raport(buffer);
        printf("%s\n", buffer);
    }
    return 0;
}