#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/msg.h>

#define K1 3
#define K2 5

int sprawdz_dlugosc_kolejki()
{
    int liczba = 0;
    sem_lock(SEM_SHARED);
    liczba = shared->liczba_oczekujacych_klientow;
    sem_unlock(SEM_SHARED);
    return liczba;
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

    int czy_aktywny = (id_pracownika ==0) ? 1 : 0;

    Msg msg;

    printf("[PRACOWNIK SERWISU %d] Uruchomiony\n", id_pracownika);

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

        if (id_pracownika != 0)
        {
            czy_aktywny = 0;
        }

        printf("[PRACOWNIK SERWISU %d] Serwis otwarty, zaczynam pracę\n", id_pracownika);

        while (1)
        {
            sem_lock(SEM_SHARED);
            if (shared->pozar || !shared->serwis_otwarty)
            {
                sem_unlock(SEM_SHARED);
                printf("[PRACOWNIK SERWISU %d] Pożar lub zamknięcie serwisu, kończę zmianę, %d:00\n", id_pracownika, shared->aktualna_godzina);
                break;
            }
            sem_unlock(SEM_SHARED);

            int q_len = sprawdz_dlugosc_kolejki();

            if (id_pracownika == 1)
            {
                if (!czy_aktywny && q_len >= K1)
                {
                    czy_aktywny = 1;
                    printf("[PRACOWNIK SERWISU %d] Kolejka %d >= %d, Otwieram 2 stanowisko\n", id_pracownika, q_len, K1);
                }
                else if (!czy_aktywny)
                {
                    sleep(1);
                    continue;
                }
                else if (czy_aktywny && q_len <= 2)
                {
                    czy_aktywny = 0;
                    printf("[PRACOWNIK SERWISU %d] Kolejka %d <= 2, Zamykam 2 stanowisko\n", id_pracownika, q_len);
                    sleep(1);
                    continue;
                }
            }
            else if (id_pracownika == 2)
            {
                if (!czy_aktywny && q_len >= K2)
                {
                    czy_aktywny = 1;
                    printf("[PRACOWNIK SERWISU %d] Kolejka %d >= %d, Otwieram 3 stanowisko\n", id_pracownika, q_len, K2);
                }
                else if (!czy_aktywny)
                {
                    sleep(1);
                    continue;
                }
                else if (czy_aktywny && q_len <= 3)
                {
                    czy_aktywny = 0;
                    printf("[PRACOWNIK SERWISU %d] Kolejka %d <= 3, Zamykam 3 stanowisko\n", id_pracownika, q_len);
                    sleep(1);
                    continue;
                }
                
            }

            int odebrano = 0;

            if (msgrcv(msg_id, &msg, sizeof(Samochod), MSG_OD_MECHANIKA, IPC_NOWAIT) != -1)
            {
                odebrano = 1;

                if (msg.samochod.dodatkowa_usterka)
                {
                    printf("[PRACOWNIK SERWISU %d] Mechanik ze stanowiska %d zgłosił dodatkową usterkę w aucie %d: %s\n", id_pracownika, msg.samochod.id_stanowiska_roboczego, msg.samochod.pid_kierowcy, pobierz_usluge(msg.samochod.id_dodatkowej_uslugi).nazwa);

                    msg.mtype = msg.samochod.pid_kierowcy;
                    msgsnd(msg_id, &msg, sizeof(Samochod), 0);
                }
                else
                {
                    printf("[PRACOWNIK SERWISU %d] Auto %d ukończone przez mechanika na stanowisku %d\n", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.id_stanowiska_roboczego);

                    msg.mtype = MSG_KASA;
                    msgsnd(msg_id, &msg, sizeof(Samochod), 0);
                }
            }

            if (msgrcv(msg_id, &msg, sizeof(Samochod), MSG_DECYZJA_DODATKOWA, IPC_NOWAIT) != -1)
            {
                odebrano = 1;
                printf("[PRACOWNIK SERWISU %d] Otrzymano decyzję od kierowcy %d. Przekazuję mechanikowi na stanowisko %d\n", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.id_stanowiska_roboczego);

                msg.mtype = 100 + msg.samochod.id_stanowiska_roboczego;
                msgsnd(msg_id, &msg, sizeof(Samochod), 0);
            }


            //Odbiór klienta 
            if (msgrcv(msg_id, &msg, sizeof(Samochod), MSG_REJESTRACJA, IPC_NOWAIT) != -1)
            {
                odebrano = 1;

                sem_lock(SEM_SHARED);
                if (shared->liczba_oczekujacych_klientow > 0)
                    shared->liczba_oczekujacych_klientow--;
                sem_unlock(SEM_SHARED);

                printf("[PRACOWNIK SERWISU %d] Obsługa kierowcy %d, marka %s, usługa ID: %d\n", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.marka, msg.samochod.id_uslugi);

                Usluga u = pobierz_usluge(msg.samochod.id_uslugi);

                //Wycena naprawy
                msg.samochod.czas_naprawy = u.czas_wykonania;
                msg.samochod.koszt = u.koszt;

                msg.samochod.zaakceptowano = 0;
                msg.samochod.dodatkowa_usterka = 0;

                msg.samochod.id_stanowiska_roboczego = -1;

                printf("[PRACOWNIK SERWISU %d] Wycena dla kierowcy %d: koszt %d, czas naprawy %d\n", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.koszt, msg.samochod.czas_naprawy);

                //Odesłanie wyceny
                msg.mtype = msg.samochod.pid_kierowcy;
                msgsnd(msg_id, &msg, sizeof(Samochod), 0);

                Msg decyzja;

                while (1)
                {
                    if (msgrcv(msg_id, &decyzja, sizeof(Samochod), MSG_DECYZJA_USLUGI, 0) != -1)
                    {
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
                            usleep(500000);
                        }
                    }

                    msg.samochod.id_stanowiska_roboczego = mechanik_id;

                    msg.mtype = 100 + mechanik_id;
                    msgsnd(msg_id, &msg, sizeof(Samochod), 0);
                    printf("[PRACOWNIK SERWISU %d] Przekazano auto %d do mechanika %d\n", id_pracownika, msg.samochod.pid_kierowcy, mechanik_id);
                }
                else
                {
                    printf("[PRACOWNIK SERWISU %d] Kierowca %d odrzucił usługę\n", id_pracownika, msg.samochod.pid_kierowcy);
                }
            }

            if (!odebrano)
            {
                usleep(500000);
            }
        }

        printf("[PRACOWNIK SERWISU %d] Koniec zmiany. Czekam na kolejny dzień...\n", id_pracownika);
    }
    return 0;
}