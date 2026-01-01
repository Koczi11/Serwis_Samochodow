#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

//Globalne ID stanowiska
int id_stanowiska = -1;

//Globalne flagi
volatile sig_atomic_t przyspieszony = 0;
volatile sig_atomic_t zamknij_po = 0;

//Obsługa sygnałów
void sig_zamknij(int sig)
{
    zamknij_po = 1;
    printf("[MECHANIK %d] Otrzymano sygnał zamknięcia stanowiska %d po obsłudze\n", getpid(), id_stanowiska);
}

void sig_przyspiesz(int sig)
{
    if (!przyspieszony)
    {
        przyspieszony = 1;
        printf("[MECHANIK %d] Otrzymano sygnał przyspieszenia stanowiska %d\n", getpid(), id_stanowiska);
    }
    else
    {
        printf("[MECHANIK %d] Stanowisko %d już jest przyspieszone\n", getpid(), id_stanowiska);
    }
}

void sig_normalnie(int sig)
{
    if (przyspieszony)
    {
        przyspieszony = 0;
        printf("[MECHANIK %d] Otrzymano sygnał normalnej pracy stanowiska %d\n", getpid(), id_stanowiska);
    }
}

void sig_pozar(int sig)
{
    printf("[MECHANIK %d] Pożar!\n", getpid());
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Brak ID stanowiska\n");
        exit(1);
    }

    id_stanowiska = atoi(argv[1]);
    init_ipc(0);

    if (signal(SIGUSR1, sig_zamknij) == SIG_ERR)
    {
        perror("signal SIGUSR1 failed");
    }

    if (signal(SIGUSR2, sig_przyspiesz) == SIG_ERR)
    {
        perror("signal SIGUSR2 failed");
    }

    if (signal(SIGCONT, sig_normalnie) == SIG_ERR)
    {
        perror("signal SIGCONT failed");
    }

    if (signal(SIGTERM, sig_pozar) == SIG_ERR)
    {
        perror("signal SIGTERM failed");
    }

    Msg msg;

    sem_lock(SEM_SHARED);
    shared->stanowiska[id_stanowiska].pid_mechanika = getpid();
    shared->stanowiska[id_stanowiska].zajete = 0;
    shared->stanowiska[id_stanowiska].przyspieszone = 0;
    sem_unlock(SEM_SHARED);

    printf("[MECHANIK %d] Stanowisko %d gotowe (Marki : %s)\n", getpid(), id_stanowiska, (id_stanowiska == 7) ? "U i Y" : "A, E, I, O, U i Y" );

    while (1)
    {
        if (zamknij_po)
        {
            printf("[MECHANIK %d] Zamykam stanowisko %d\n", getpid(), id_stanowiska);
            sem_lock(SEM_SHARED);
            shared->stanowiska[id_stanowiska].pid_mechanika = -1;
            sem_unlock(SEM_SHARED);
            exit(0);
        }

        if (msgrcv(msg_id, &msg, sizeof(Samochod), 100 + id_stanowiska, 0) == -1)
        {
            if (zamknij_po)
                continue;
            perror("[MECHANIK] Błąd odbioru wiadomości");
            exit(1);
        }

        sem_lock(SEM_SHARED);
        shared->stanowiska[id_stanowiska].zajete = 1;
        sem_unlock(SEM_SHARED);

        printf("[MECHANIK %d] Naprawiam auto %d (Marka: %s)\n", getpid(), msg.samochod.pid_kierowcy, msg.samochod.marka);

        int czas_bazowy = msg.samochod.czas_naprawy;

        int part1 = czas_bazowy / 2;

        if (przyspieszony)
            part1 /= 2;
        
        if (part1 < 1)
            part1 = 1;

        sleep(part1);

        if (rand() % 5 == 0)
        {
            printf("[MECHANIK %d] Wykryto dodatkową usterkę w aucie %d\n", getpid(), msg.samochod.pid_kierowcy);

            msg.samochod.dodatkowa_usterka = 1;
            msg.samochod.dodatkowy_czas = 2 + rand() % 5;
            msg.samochod.dodatkowy_koszt = 50 + rand() % 200;
            msg.samochod.zaakceptowano = 0;

            msg.mtype = msg.samochod.pid_kierowcy;
            msgsnd(msg_id, &msg, sizeof(Samochod), 0);

            Msg odp;

            while (1)
            {
                if (msgrcv(msg_id, &odp, sizeof(Samochod), MSG_ODPOWIEDZ, 0) == -1)
                {
                    break;
                }
                
                if (odp.samochod.pid_kierowcy == msg.samochod.pid_kierowcy)
                {
                    msg = odp;
                    break;
                }

                msgsnd(msg_id, &odp, sizeof(Samochod), 0);
            }

            if(msg.samochod.zaakceptowano)
            {
                printf("[MECHANIK %d] Dodatkowa naprawa zaakceptowana (+%ds)\n", getpid(), msg.samochod.dodatkowy_czas);
                czas_bazowy += msg.samochod.dodatkowy_czas;
                msg.samochod.koszt += msg.samochod.dodatkowy_koszt;
            }
            else
            {
                printf("[MECHANIK %d] Dodatkowa naprawa odrzucona\n", getpid());
            }
        }

        int part2 = czas_bazowy - part1;

        if (przyspieszony)
            part2 /= 2;

        if (part2 > 0)
            sleep(part2);

        printf("[MECHANIK %d] Koniec naprawy auta %d. Koszt: %d PLN\n", getpid(), msg.samochod.pid_kierowcy, msg.samochod.koszt);

        msg.mtype = msg.samochod.pid_kierowcy;
        msg.samochod.dodatkowa_usterka = 0;
        msgsnd(msg_id, &msg, sizeof(Samochod), 0);

        sem_lock(SEM_SHARED);
        shared->stanowiska[id_stanowiska].zajete = 0;
        sem_unlock(SEM_SHARED);
    }
    return 0;
}