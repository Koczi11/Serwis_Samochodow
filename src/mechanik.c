#define _GNU_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/msg.h>

//Zmienne globalne
int id_stanowiska = -1;

//Flagi obsługi sygnałów
volatile sig_atomic_t przyspieszony = 0;
volatile sig_atomic_t zamknij_po = 0;
volatile sig_atomic_t jest_pozar = 0;

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
    jest_pozar = 1;
    const char *msg = "[MECHANIK] POŻAR!\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

//Funkcja wykonująca pracę mechanika
//Uwzględnia tryb przyspieszony oraz ewentualny pożar
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

    //Pętla sleep z obsługą przerwań
    while (czas_pracy > 0)
    {
        if (jest_pozar)
        {
            return;
        }

        unsigned int unslept = sleep(czas_pracy);

        if (jest_pozar)
        {
            return;
        }

        if (unslept == 0)
        {
            //Cały czas minął poprawnie
            break;
        }
        else
        {
            //Przerwano sleep
            czas_pracy = unslept;

            //Dostosowanie czasu pracy w trybie przyspieszonym
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

    //Dołączenie do IPC
    init_ipc(0);

    //Ustawienie obsługi sygnałów
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

    //Złoszenie obecności w pamięci współdzielonej
    sem_lock(SEM_SHARED);
    shared->stanowiska[id_stanowiska].pid_mechanika = getpid();
    sem_unlock(SEM_SHARED);

    printf("[MECHANIK %d] Stanowisko %d (Marki : %s)\n", getpid(), id_stanowiska, (id_stanowiska == 7) ? "U i Y" : "A, E, I, O, U i Y" );

    //Pętla dni pracy
    while (1)
    {
        if (jest_pozar)
            break;

        //Czekanie na otwarcie serwisu
        int czy_otwarte = 0;
        while (!czy_otwarte)
        {
            if (jest_pozar)
                break;

            sem_lock(SEM_SHARED);
            czy_otwarte = shared->serwis_otwarty;
            sem_unlock(SEM_SHARED);
            if (!czy_otwarte)
            {
                sleep(1);
            }
        }

        if (jest_pozar)
            break;

        printf("[MECHANIK %d] Serwis otwarty, przygotowuję stanowisko %d\n", getpid(), id_stanowiska);

        //Reset stanu stanowiska na początek dnia
        sem_lock(SEM_SHARED);
        shared->stanowiska[id_stanowiska].zajete = 0;
        sem_unlock(SEM_SHARED);

        //Pętla obsługi aut w ciągu dnia
        while (1)
        {
            if (jest_pozar)
                break;

            //Sprawdzenie czy serwis jest nadal otwarty
            sem_lock(SEM_SHARED);
            int otwarte = shared->serwis_otwarty;
            sem_unlock(SEM_SHARED);

            if (!otwarte)
            {
                printf("[MECHANIK %d] Serwis zamknięty, zamykam stanowisko %d\n", getpid(), id_stanowiska);
                break;
            }

            //Obsługa zamknięcia stanowiska
            if (zamknij_po)
            {
                //Sprawdzenie czy są jeszcze auta do obsłużenia
                if (msgrcv(msg_id, &msg, sizeof(Samochod), 100 + id_stanowiska, IPC_NOWAIT) != -1)
                {
                    //Obsługa auta przed zamknięciem stanowiska
                    msg.mtype = MSG_REJESTRACJA;
                    msg.samochod.id_stanowiska_roboczego = -1;

                    //Zwrócenie auta do kolejki oczekujących
                    sem_lock(SEM_SHARED);
                    shared->liczba_oczekujacych_klientow++;
                    sem_unlock(SEM_SHARED);

                    if (msgsnd(msg_id, &msg, sizeof(Samochod), 0) == -1)
                    {
                        perror("[MECHANIK] Błąd odsyłania samochodu do kolejki");
                    }
                    else
                    {
                        printf("[MECHANIK %d] Auto %d wróciło do kolejki oczekujących\n", getpid(), msg.samochod.pid_kierowcy);
                    }
                }

                printf("[MECHANIK %d] Zamykam stanowisko %d\n", getpid(), id_stanowiska);

                //Zamknięcie stanowiska
                sem_lock(SEM_SHARED);
                shared->stanowiska[id_stanowiska].pid_mechanika = -1;
                shared->stanowiska[id_stanowiska].zajete = 0;
                sem_unlock(SEM_SHARED);
                exit(0);
            }

            //Oczekiwanie na zlecenie naprawy auta
            if (msgrcv(msg_id, &msg, sizeof(Samochod), 100 + id_stanowiska, IPC_NOWAIT) == -1)
            {
                if (errno == ENOMSG || errno == EINTR)
                {
                    if (jest_pozar)
                        break;

                    //Sygnał zamknięcia stanowiska podczas czekania
                    if (zamknij_po && errno != EINTR)
                    {
                        //Zamknięcie stanowiska podczas oczekiwania
                        printf("[MECHANIK %d] Zamykam stanowisko %d\n", getpid(), id_stanowiska);
                        sem_lock(SEM_SHARED);
                        shared->stanowiska[id_stanowiska].pid_mechanika = -1;
                        sem_unlock(SEM_SHARED);
                        exit(0);
                    }

                    if (errno == ENOMSG)
                        usleep(100000);
                    continue;
                }

                perror("[MECHANIK] Błąd odbioru wiadomości");
                exit(1);
            }

            //Rozpoczęcie naprawy auta
            sem_lock(SEM_SHARED);
            shared->stanowiska[id_stanowiska].zajete = 1;
            sem_unlock(SEM_SHARED);

            printf("[MECHANIK %d] Naprawiam auto %d (Marka: %s)\n", getpid(), msg.samochod.pid_kierowcy, msg.samochod.marka);

            int czas_bazowy = msg.samochod.czas_naprawy;
            int part1 = czas_bazowy / 2;

            wykonaj_prace(part1);

            if (jest_pozar)
            {
                //Procedura ewakuacji podczas pożaru
                printf("[MECHANIK %d] Pożar! Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!\n", getpid(), msg.samochod.pid_kierowcy);
                msg.mtype = msg.samochod.pid_kierowcy;
                msg.samochod.ewakuacja = 1;
                msg.samochod.koszt = 0;
                msgsnd(msg_id, &msg, sizeof(Samochod), 0);

                sem_lock(SEM_SHARED);
                shared->stanowiska[id_stanowiska].zajete = 0;
                sem_unlock(SEM_SHARED);
                break;
            }

            //20% szans na dodatkową usterkę
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

                //Wysyłamy do Pracownika Serwisu
                msg.mtype = MSG_OD_MECHANIKA;
                msgsnd(msg_id, &msg, sizeof(Samochod), 0);

                printf("[MECHANIK %d] Zgłoszono dodatkową usterkę do Pracownika Serwisu\n", getpid());

                Msg odp;

                //Czekanie na decyzję kierowcy
                while (1)
                {
                    if (jest_pozar)
                    {
                        break;
                    }

                    if (msgrcv(msg_id, &odp, sizeof(Samochod), 100 + id_stanowiska, IPC_NOWAIT) != -1)
                    {
                        //Sprawdzenie czy to odpowiedź dla tego auta
                        if (odp.samochod.pid_kierowcy == msg.samochod.pid_kierowcy)
                        {
                            msg = odp;
                            break;
                        }
                        else
                        {
                            msgsnd(msg_id, &odp, sizeof(Samochod), 0);
                        }
                    }
                    else
                    {
                        usleep(100000);
                    }
                }

                if (jest_pozar)
                {
                    //Procedura ewakuacji podczas pożaru
                    printf("[MECHANIK %d] Pożar! Nie czekam na decyzję. Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!\n", getpid(), msg.samochod.pid_kierowcy);
                    msg.mtype = msg.samochod.pid_kierowcy;
                    msg.samochod.ewakuacja = 1;
                    msg.samochod.koszt = 0;
                    msgsnd(msg_id, &msg, sizeof(Samochod), 0);

                    sem_lock(SEM_SHARED);
                    shared->stanowiska[id_stanowiska].zajete = 0;
                    sem_unlock(SEM_SHARED);
                    break;
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

            //Dokończenie pracy nad autem
            int czas_calkowity = czas_bazowy;
            int part2 = czas_calkowity - part1;

            if (part2 > 0)
                wykonaj_prace(part2);

            if (jest_pozar)
            {
                printf("[MECHANIK %d] Pożar! Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!\n", getpid(), msg.samochod.pid_kierowcy);
                msg.mtype = msg.samochod.pid_kierowcy;
                msg.samochod.ewakuacja = 1;
                msg.samochod.koszt = 0;
                msgsnd(msg_id, &msg, sizeof(Samochod), 0);

                sem_lock(SEM_SHARED);
                shared->stanowiska[id_stanowiska].zajete = 0;
                sem_unlock(SEM_SHARED);
                break;
            }

            //Koniec naprawy
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

        if (jest_pozar)
        {
            printf("[MECHANIK %d] Uciekam przed pożarem!\n", getpid());
            break;
        }

        printf("[MECHANIK %d] Czekam na kolejny dzień...\n", getpid());
        sleep(1);
    }
    return 0;
}