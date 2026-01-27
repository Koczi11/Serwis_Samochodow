#define _GNU_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/msg.h>
#include <time.h>
#include <sys/wait.h>

//Zmienne globalne
int id_stanowiska = -1;

//Flagi obsługi sygnałów
volatile sig_atomic_t przyspieszony = 0;
volatile sig_atomic_t zamknij_po = 0;
volatile sig_atomic_t ewakuacja = 0;

//Obsługa sygnałów
void sig_zamknij(int sig)
{
    (void)sig;
    zamknij_po = 1;
    const char *msg = "[MECHANIK] Otrzymano sygnał zamknięcia stanowiska po obsłudze\n";
    if (write(STDOUT_FILENO, msg, strlen(msg)) == -1)
    {
        perror("write failed");
    }
}

void sig_przyspiesz(int sig)
{
    (void)sig;
    if (!przyspieszony)
    {
        przyspieszony = 1;
        const char *msg = "[MECHANIK] Otrzymano sygnał przyspieszenia stanowiska\n";
        if (write(STDOUT_FILENO, msg, strlen(msg)) == -1)
        {
            perror("write failed");
        }
    }
    else
    {
        const char *msg = "[MECHANIK] Stanowisko już jest w trybie przyspieszonym\n";
        if (write(STDOUT_FILENO, msg, strlen(msg)) == -1)
        {
            perror("write failed");
        }
    }
}

void sig_normalnie(int sig)
{
    (void)sig;
    if (przyspieszony)
    {
        przyspieszony = 0;
        const char *msg = "[MECHANIK] Otrzymano sygnał powrotu do normalnego trybu stanowiska\n";
        if (write(STDOUT_FILENO, msg, strlen(msg)) == -1)
        {
            perror("write failed");
        }
    }
}

void handle_pozar(int sig)
{
    (void)sig;
    ewakuacja = 1;
}

//Funkcja wykonująca pracę mechanika
void wykonaj_prace(int czas_pracy)
{
    double wykonano = 0.0;
    double krok = 0.1;

    while (wykonano < czas_pracy)
    {
        if (ewakuacja)
        {
            break;
        }

        if (safe_wait_seconds(krok) == -1)
        {
            if (ewakuacja)
            {
                break;
            }
        }

        wykonano += (przyspieszony ? krok * 2.0 : krok);
    }
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        char arg_buff[16];

        for (int i = 0; i < MAX_STANOWISK; i++)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                snprintf(arg_buff, sizeof(arg_buff), "%d", i);
                execl("./mechanik", "mechanik", arg_buff, NULL);
                perror("execl mechanik failed");
                exit(1);
            }
            else if (pid < 0)
            {
                perror("fork mechanik failed");
                exit(1);
            }
        }

        while (wait(NULL) > 0);
        return 0;
    }

    srand(getpid() ^ time(NULL));

    if (argc < 2)
    {
        fprintf(stderr, "Brak ID stanowiska\n");
        exit(1);
    }

    id_stanowiska = atoi(argv[1]);

    //Dołączenie do IPC
    init_ipc(0);
    join_service_group();

    //Ustawienie obsługi sygnałów
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sa.sa_handler = sig_zamknij;
    if (sigaction(SIGRTMIN, &sa, NULL) == -1)
    {
        perror("sigaction SIGRTMIN failed");
    }

    sa.sa_handler = sig_przyspiesz;
    if (sigaction(SIGRTMIN + 1, &sa, NULL) == -1)
    {
        perror("sigaction SIGRTMIN+1 failed");
    }

    sa.sa_handler = sig_normalnie;
    if (sigaction(SIGRTMIN + 2, &sa, NULL) == -1)
    {
        perror("sigaction SIGRTMIN+2 failed");
    }

    sa.sa_handler = handle_pozar;
    if (sigaction(SIGUSR1, &sa, NULL) == -1)
    {
        perror("sigaction SIGUSR1 failed");
    }

    Msg msg;
    char buffer[256];
    int dodatkowy_koszt_zaakceptowany = 0;
    int dodatkowy_id_zaakceptowany = -1;
    int w_trakcie_naprawy = 0;

    //Złoszenie obecności w pamięci współdzielonej
    sem_lock(SEM_STANOWISKA);
    shared->stanowiska[id_stanowiska].pid_mechanika = getpid();
    sem_unlock(SEM_STANOWISKA);

    printf("[MECHANIK %d] Stanowisko %d (Marki : %s)\n", getpid(), id_stanowiska, (id_stanowiska == 7) ? "U i Y" : "A, E, I, O, U i Y" );
    snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Stanowisko %d (Marki : %s)", getpid(), id_stanowiska, (id_stanowiska == 7) ? "U i Y" : "A, E, I, O, U i Y" );
    zapisz_log(buffer);

    //Pętla dni pracy
    while (1)
    {
        if (ewakuacja)
        {
            printf("[MECHANIK %d] Uciekam przed pożarem! Czekam na ugaszenie i następny dzień.\n", getpid());
            snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Uciekam przed pożarem!", getpid());
            zapisz_log(buffer);

            sem_lock(SEM_STANOWISKA);
            shared->stanowiska[id_stanowiska].zajete = 0;
            sem_unlock(SEM_STANOWISKA);

            ewakuacja = 0;
        }

        //Czekanie na otwarcie serwisu (komunikat)
        while (1)
        {
            int r = recv_msg(msg_id_mechanik, &msg, MSG_CTRL_OPEN_MECHANIK, 0);
            if (r == 0)
            {
                break;
            }
            if (r == -2)
            {
                exit(0);
            }

            if (errno == EINTR && ewakuacja)
            {
                break;
            }
        }

        if (ewakuacja)
        {
            continue;
        }

        printf("[MECHANIK %d] Serwis otwarty, przygotowuję stanowisko %d\n", getpid(), id_stanowiska);
        snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Serwis otwarty, przygotowuję stanowisko %d", getpid(), id_stanowiska);
        zapisz_log(buffer);

        //Reset stanu stanowiska na początek dnia
        sem_lock(SEM_STANOWISKA);
        shared->stanowiska[id_stanowiska].zajete = 0;
        sem_unlock(SEM_STANOWISKA);

        //Pętla obsługi aut w ciągu dnia
        while (1)
        {
            if (ewakuacja)
            {
                break;
            }

            //Sprawdzenie czy serwis jest nadal otwarty
            sem_lock(SEM_STATUS);
            int otwarte = shared->serwis_otwarty;
            sem_unlock(SEM_STATUS);

            if (!otwarte)
            {
                printf("[MECHANIK %d] Serwis zamknięty, zamykam stanowisko %d\n", getpid(), id_stanowiska);
                snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Serwis zamknięty, zamykam stanowisko %d", getpid(), id_stanowiska);
                zapisz_log(buffer);
                
                break;
            }

            //Obsługa zamknięcia stanowiska
            if (zamknij_po)
            {
                //Sprawdzenie czy są jeszcze auta do obsłużenia
                if (recv_msg(msg_id_mechanik, &msg, 100 + id_stanowiska, IPC_NOWAIT) != -1)
                {
                    //Obsługa auta przed zamknięciem stanowiska
                    msg.mtype = MSG_REJESTRACJA;
                    msg.samochod.id_stanowiska_roboczego = -1;
                    sem_lock(SEM_LICZNIKI);
                    shared->liczba_oczekujacych_klientow++;
                    sem_unlock(SEM_LICZNIKI);

                    printf("[MECHANIK %d] Auto %d wróciło do kolejki oczekujących\n", getpid(), msg.samochod.pid_kierowcy);
                    snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Auto %d wróciło do kolejki oczekujących", getpid(), msg.samochod.pid_kierowcy);
                    zapisz_log(buffer);
                    
                    if (send_msg(msg_id_kierowca, &msg) == -1)
                    {
                        perror("[MECHANIK] Błąd odsyłania samochodu do kolejki");
                    }
                }

                printf("[MECHANIK %d] Zamykam stanowisko %d\n", getpid(), id_stanowiska);
                snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Zamykam stanowisko %d", getpid(), id_stanowiska);
                zapisz_log(buffer);

                //Zamknięcie stanowiska
                sem_lock(SEM_STANOWISKA);
                shared->stanowiska[id_stanowiska].pid_mechanika = -1;
                shared->stanowiska[id_stanowiska].zajete = 0;
                sem_unlock(SEM_STANOWISKA);
                exit(0);
            }

            //Oczekiwanie na zlecenie naprawy auta
            int got_msg = 0;
            while (1)
            {
                if (ewakuacja)
                {
                    break;
                }

                sem_lock(SEM_STATUS);
                int otwarte_po = shared->serwis_otwarty;
                sem_unlock(SEM_STATUS);
                if (!otwarte_po)
                {
                    break;
                }

                int r = recv_msg(msg_id_mechanik, &msg, 100 + id_stanowiska, IPC_NOWAIT);
                if (r == 0)
                {
                    got_msg = 1;
                    break;
                }
                if (r == -2)
                {
                    exit(0);
                }

                if (errno == EINTR)
                {
                    continue;
                }

                safe_wait_seconds(0.2);
            }

            if (ewakuacja)
            {
                break;
            }

            if (!got_msg)
            {
                continue;
            }

            printf("[MECHANIK %d] Otrzymano zlecenie naprawy auta %d\n", getpid(), msg.samochod.pid_kierowcy);
            snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Otrzymano zlecenie naprawy auta %d", getpid(), msg.samochod.pid_kierowcy);
            zapisz_log(buffer);

            dodatkowy_koszt_zaakceptowany = 0;
            dodatkowy_id_zaakceptowany = -1;
            w_trakcie_naprawy = 1;
            
            //Rozpoczęcie naprawy auta
            sem_lock(SEM_STANOWISKA);
            shared->stanowiska[id_stanowiska].zajete = 1;
            sem_unlock(SEM_STANOWISKA);

            double czas_calkowity = (double)msg.samochod.czas_naprawy;
            int part1 = czas_calkowity / 2.0;

            wykonaj_prace(part1);

            if (ewakuacja)
            {
                //Procedura ewakuacji podczas pożaru
                printf("[MECHANIK %d] Pożar! Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!\n", getpid(), msg.samochod.pid_kierowcy);
                snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Pożar! Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!", getpid(), msg.samochod.pid_kierowcy);
                zapisz_log(buffer);

                if (w_trakcie_naprawy)
                {
                    sem_lock(SEM_LICZNIKI);
                    if (shared->auta_w_serwisie > 0)
                    {
                        shared->auta_w_serwisie--;
                    }
                    sem_unlock(SEM_LICZNIKI);
                    w_trakcie_naprawy = 0;
                }
                
                msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                msg.samochod.ewakuacja = 1;
                msg.samochod.koszt = 0;

                if (send_msg(msg_id_kierowca, &msg) == -1)
                {
                    perror("[MECHANIK] Błąd wysyłania wiadomości o ewakuacji");
                }

                sem_lock(SEM_STANOWISKA);
                shared->stanowiska[id_stanowiska].zajete = 0;
                sem_unlock(SEM_STANOWISKA);
                break;
            }

            //20% szans na dodatkową usterkę
            if (rand() % 5 == 0)
            {
                int dodatkowa_id = rand() % MAX_USLUG;
                Usluga dodatkowa = pobierz_usluge(dodatkowa_id);

                printf("[MECHANIK %d] Wykryto dodatkową usterkę w aucie %d: %s\n", getpid(), msg.samochod.pid_kierowcy, dodatkowa.nazwa);
                snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Wykryto dodatkową usterkę w aucie %d: %s", getpid(), msg.samochod.pid_kierowcy, dodatkowa.nazwa);
                zapisz_log(buffer);

                msg.samochod.dodatkowa_usterka = 1;
                msg.samochod.id_dodatkowej_uslugi = dodatkowa_id;
                msg.samochod.dodatkowy_czas = dodatkowa.czas_wykonania;
                msg.samochod.dodatkowy_koszt = dodatkowa.koszt;
                msg.samochod.zaakceptowano = 0;
                msg.samochod.id_stanowiska_roboczego = id_stanowiska;

                //Wysyłamy do Pracownika Serwisu
                msg.mtype = MSG_OD_MECHANIKA;
                send_msg(msg_id_mechanik, &msg);

                printf("[MECHANIK %d] Zgłoszono dodatkową usterkę do Pracownika Serwisu\n", getpid());
                snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Zgłoszono dodatkową usterkę do Pracownika Serwisu", getpid());
                zapisz_log(buffer);

                Msg odp;

                //Czekanie na decyzję kierowcy
                while (1)
                {
                    if (ewakuacja)
                    {
                        break;
                    }

                    int r = recv_msg(msg_id_mechanik, &odp, 100 + id_stanowiska, 0);
                    if (r == -2)
                    {
                        exit(0);
                    }
                    if (r != -1)
                    {
                        //Sprawdzenie czy to odpowiedź dla tego auta
                        if (odp.samochod.pid_kierowcy == msg.samochod.pid_kierowcy)
                        {
                            msg = odp;
                            break;
                        }
                        else
                        {
                            send_msg(msg_id_mechanik, &odp);
                        }
                    }
                    else if (errno != EINTR)
                    {
                        perror("[MECHANIK] Błąd odbierania decyzji");
                        exit(1);
                    }
                }

                if (ewakuacja)
                {
                    //Procedura ewakuacji podczas pożaru
                    printf("[MECHANIK %d] Pożar! Nie czekam na decyzję. Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!\n", getpid(), msg.samochod.pid_kierowcy);
                    snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Pożar! Nie czekam na decyzję. Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!", getpid(), msg.samochod.pid_kierowcy);
                    zapisz_log(buffer);

                    if (w_trakcie_naprawy)
                    {
                        sem_lock(SEM_LICZNIKI);
                        if (shared->auta_w_serwisie > 0)
                        {
                            shared->auta_w_serwisie--;
                        }
                        sem_unlock(SEM_LICZNIKI);
                        w_trakcie_naprawy = 0;
                    }
                    
                    msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                    msg.samochod.ewakuacja = 1;
                    msg.samochod.koszt = 0;
                    send_msg(msg_id_kierowca, &msg);

                    sem_lock(SEM_STANOWISKA);
                    shared->stanowiska[id_stanowiska].zajete = 0;
                    sem_unlock(SEM_STANOWISKA);
                    break;
                }

                if(msg.samochod.zaakceptowano)
                {
                    printf("[MECHANIK %d] Dodatkowa naprawa zaakceptowana (+%ds)\n", getpid(), msg.samochod.dodatkowy_czas);
                    snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Dodatkowa naprawa zaakceptowana (+%ds)", getpid(), msg.samochod.dodatkowy_czas);
                    zapisz_log(buffer);

                    czas_calkowity += (double)msg.samochod.dodatkowy_czas;
                    dodatkowy_koszt_zaakceptowany = msg.samochod.dodatkowy_koszt;
                    dodatkowy_id_zaakceptowany = msg.samochod.id_dodatkowej_uslugi;
                }
                else
                {
                    printf("[MECHANIK %d] Dodatkowa naprawa odrzucona\n", getpid());
                    snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Dodatkowa naprawa odrzucona", getpid());
                    zapisz_log(buffer);
                    dodatkowy_koszt_zaakceptowany = 0;
                    dodatkowy_id_zaakceptowany = -1;
                }
            }

            //Dokończenie pracy nad autem
            double part2 = czas_calkowity - part1;

            if (part2 > 0)
            {
                wykonaj_prace(part2);
            }
            
            if (ewakuacja)
            {
                printf("[MECHANIK %d] Pożar! Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!\n", getpid(), msg.samochod.pid_kierowcy);
                snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Pożar! Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!", getpid(), msg.samochod.pid_kierowcy);
                zapisz_log(buffer);

                if (w_trakcie_naprawy)
                {
                    sem_lock(SEM_LICZNIKI);
                    if (shared->auta_w_serwisie > 0)
                    {
                        shared->auta_w_serwisie--;
                    }
                    sem_unlock(SEM_LICZNIKI);
                    w_trakcie_naprawy = 0;
                }

                msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                msg.samochod.ewakuacja = 1;
                msg.samochod.koszt = 0;
                send_msg(msg_id_kierowca, &msg);

                sem_lock(SEM_STANOWISKA);
                shared->stanowiska[id_stanowiska].zajete = 0;
                sem_unlock(SEM_STANOWISKA);
                break;
            }

            //Koniec naprawy
            printf("[MECHANIK %d] Koniec naprawy auta %d. Przekazuję zakres prac\n", getpid(), msg.samochod.pid_kierowcy);
            snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Koniec naprawy auta %d. Przekazuję zakres prac", getpid(), msg.samochod.pid_kierowcy);
            zapisz_log(buffer);

            msg.mtype = MSG_OD_MECHANIKA;
            msg.samochod.dodatkowa_usterka = 0;
            msg.samochod.id_stanowiska_roboczego = id_stanowiska;
            msg.samochod.dodatkowy_koszt = dodatkowy_koszt_zaakceptowany;
            msg.samochod.id_dodatkowej_uslugi = dodatkowy_id_zaakceptowany;
            
            send_msg(msg_id_mechanik, &msg);

            w_trakcie_naprawy = 0;

            sem_lock(SEM_STANOWISKA);
            shared->stanowiska[id_stanowiska].zajete = 0;
            sem_unlock(SEM_STANOWISKA);

            Msg wolny;
            wolny.mtype = MSG_WOLNY_MECHANIK;
            wolny.samochod.id_stanowiska_roboczego = id_stanowiska;
            send_msg(msg_id_mechanik, &wolny);
        }

        sem_lock(SEM_STANOWISKA);
        shared->stanowiska[id_stanowiska].zajete = 0;
        sem_unlock(SEM_STANOWISKA);

        if (ewakuacja)
        {
            printf("[MECHANIK %d] Uciekam przed pożarem! Czekam na ugaszenie i następny dzień.\n", getpid());
            snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Uciekam przed pożarem!", getpid());
            zapisz_log(buffer);

            continue;
        }

        printf("[MECHANIK %d] Czekam na kolejny dzień...\n", getpid());
        snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Czekam na kolejny dzień...", getpid());
        zapisz_log(buffer);
    }
    return 0;
}