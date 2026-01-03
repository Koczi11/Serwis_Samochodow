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
    (void)sig;
    zamknij_po = 1;
    const char *msg = "[MECHANIK] Otrzymano sygnał zamknięcia stanowiska po obsłudze\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

void sig_przyspiesz(int sig)
{
    (void)sig;
    if (!przyspieszony)
    {
        przyspieszony = 1;
        const char *msg = "[MECHANIK] Otrzymano sygnał przyspieszenia stanowiska\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    }
    else
    {
        const char *msg = "[MECHANIK] Stanowisko już jest w trybie przyspieszonym\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    }
}

void sig_normalnie(int sig)
{
    (void)sig;
    if (przyspieszony)
    {
        przyspieszony = 0;
        const char *msg = "[MECHANIK] Otrzymano sygnał powrotu do normalnego trybu stanowiska\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    }
}

void sig_pozar(int sig)
{
    (void)sig;
    const char *msg = "[MECHANIK] POŻAR!\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    exit(0);
}

void wykonaj_prace(int czas_pracy)
{
    if (przyspieszony)
    {
        czas_pracy /= 2;
    }

    if (czas_pracy < 1)
    {
        czas_pracy = 1;
    }

    while (czas_pracy > 0)
    {
        unsigned int unslept = sleep(czas_pracy);

        if (unslept == 0)
        {
            break;
        }
        else
        {
            czas_pracy = unslept;

            if (przyspieszony)
            {
                czas_pracy /= 2;
                if (czas_pracy == 0)
                {
                    czas_pracy = 1;
                }
            }
        }
    }
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
    sem_unlock(SEM_SHARED);

    printf("[MECHANIK %d] Stanowisko %d (Marki : %s)\n", getpid(), id_stanowiska, (id_stanowiska == 7) ? "U i Y" : "A, E, I, O, U i Y" );

    while (1)
    {
        int czy_otwarte = 0;
        while (!czy_otwarte)
        {
            sem_lock(SEM_SHARED);
            czy_otwarte = shared->serwis_otwarty;
            sem_unlock(SEM_SHARED);
            if (!czy_otwarte)
            {
                sleep(1);
            }
        }

        printf("[MECHANIK %d] Serwis otwarty, przygotowuję stanowisko %d\n", getpid(), id_stanowiska);

        sem_lock(SEM_SHARED);
        shared->stanowiska[id_stanowiska].zajete = 0;
        sem_unlock(SEM_SHARED);

        while (1)
        {
            sem_lock(SEM_SHARED);
            int pozar = shared->pozar;
            int otwarte = shared->serwis_otwarty;
            sem_unlock(SEM_SHARED);

            if (pozar)
            {
                printf("[MECHANIK %d] Pożar!\n", getpid());
                break;
            }

            if (!otwarte)
            {
                printf("[MECHANIK %d] Serwis zamknięty, zamykam stanowisko %d\n", getpid(), id_stanowiska);
                break;
            }

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

            wykonaj_prace(part1);

            if (rand() % 5 == 0)
            {
                int dodatkowa_id = rand() % MAX_USLUG;
                Usluga dodatkowa = pobierz_usluge(dodatkowa_id);

                printf("[MECHANIK %d] Wykryto dodatkową usterkę w aucie %d: %s\n", getpid(), msg.samochod.pid_kierowcy, dodatkowa.nazwa);

                msg.samochod.dodatkowa_usterka = 1;
                msg.samochod.id_dodatkowej_uslugi = dodatkowa_id;
                msg.samochod.dodatkowy_czas = dodatkowa.czas_wykonania;
                msg.samochod.dodatkowy_koszt = dodatkowa.koszt;
                msg.samochod.zaakceptowano = 0;

                msg.samochod.id_stanowiska_roboczego = id_stanowiska;

                msg.mtype = MSG_OD_MECHANIKA;
                msgsnd(msg_id, &msg, sizeof(Samochod), 0);

                printf("[MECHANIK %d] Zgłoszono dodatkową usterkę do Pracownika Serwisu\n", getpid());

                Msg odp;

                while (1)
                {
                    if (msgrcv(msg_id, &odp, sizeof(Samochod), 100 + id_stanowiska, 0) == -1)
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

            int czas_calkowity = czas_bazowy;
            int part2 = czas_calkowity - part1;

            if (part2 > 0)
                wykonaj_prace(part2);

            printf("[MECHANIK %d] Koniec naprawy auta %d. Koszt: %d PLN\n", getpid(), msg.samochod.pid_kierowcy, msg.samochod.koszt);

            msg.mtype = MSG_OD_MECHANIKA;
            msg.samochod.dodatkowa_usterka = 0;
            msg.samochod.id_stanowiska_roboczego = id_stanowiska;
            
            msgsnd(msg_id, &msg, sizeof(Samochod), 0);

            sem_lock(SEM_SHARED);
            shared->stanowiska[id_stanowiska].zajete = 0;
            sem_unlock(SEM_SHARED);
        }

        sem_lock(SEM_SHARED);
        shared->stanowiska[id_stanowiska].zajete = 0;
        sem_unlock(SEM_SHARED);

        printf("[MECHANIK %d] Czekam na kolejny dzień...\n", getpid());
        sleep(1);
    }
    return 0;
}