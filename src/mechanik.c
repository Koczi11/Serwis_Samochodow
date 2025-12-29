#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

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

//Globalne flagi
volatile __sig_atomic_t przyspieszony = 0;
volatile __sig_atomic_t zamknij_po = 0;

//Obsługa sygnałów
void sig_zamknij(int sig)
{
    zamknij_po = 1;
}

void sig_przyspiesz(int sig)
{
    przyspieszony = 1;
}

void sig_normalnie(int sig)
{
    przyspieszony = 0;
}

void sig_pozar(int sig)
{
    exit(0);
}

int main()
{
    signal(SIGUSR1, sig_zamknij);
    signal(SIGUSR2, sig_przyspiesz);
    signal(SIGCONT, sig_normalnie);
    signal(SIGTERM, sig_pozar);


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

        int czas = msg.samochod.czas_naprawy;
        
        if (rand() % 5 == 0) //20% szans na dodatkową usterkę
        {
            printf("[MECHANIK %d] Dodatkowa usterka auta %d!\n", getpid(), msg.samochod.pid_kierowcy);

            msg.samochod.dodatkowa_usterka = 1;
            msg.samochod.dodatkowy_czas = 3 + rand() % 5;
            msg.samochod.dodatkowy_koszt = 100 + rand() % 200;

            msg.mtype = MSG_USTERKA;
            msgsnd(msg_id, &msg, sizeof(Samochod), 0);

            //Oczekiwanie na decyzję
            msgrcv(msg_id, &msg, sizeof(Samochod), MSG_DECYZJA, 0);

            if (msg.samochod.zaakceptowano)
            {
                printf("[MECHANIK %d] Kierowca %d zaakceptował dodatkową usterkę\n", getpid(), msg.samochod.pid_kierowcy);
                czas += msg.samochod.dodatkowy_czas;
            }
            else
            {
                printf("[MECHANIK %d] Kierowca %d odrzucił dodatkową usterkę\n", getpid(), msg.samochod.pid_kierowcy);
            }
        }

        //Symulacja naprawy
        int t = przyspieszony ? czas / 2 : czas;
        sleep(t);

        //Zwolnienie stanowiska
        sem_lock(SEM_SHARED);
        shared->stanowiska[s].zajete = 0;
        shared->stanowiska[s].pid_mechanika = -1;
        sem_unlock(SEM_SHARED);

        printf("[MECHANIK %d] Zakończono naprawę auta %d\n", getpid(), msg.samochod.pid_kierowcy);

        if (zamknij_po)
        {
            printf("[MECHANIK %d] Stanowisko zamknięte\n", getpid());
            exit(0);
        }

        //Informacja o zakończeniu naprawy
        msg.mtype = MSG_KONIEC_NAPRAWY;
        msgsnd(msg_id, &msg, sizeof(Samochod), 0);

        //Przekazanie do kasy
        msg.mtype = MSG_KASA;
        msgsnd(msg_id, &msg, sizeof(Samochod), 0);
    }
}