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

void sig_pozar(int sig)
{
    (void)sig;
    jest_pozar = 1;
    const char *msg = "[MECHANIK] POŻAR!\n";
    if (write(STDOUT_FILENO, msg, strlen(msg)) == -1)
    {
        perror("write failed");
    }
}

//Funkcja wykonująca pracę mechanika
//Uwzględnia tryb przyspieszony oraz ewentualny pożar
void wykonaj_prace(int czas_pracy)
{
    double wykonano = 0.0;
    double krok = 0.1;

    while (wykonano < czas_pracy)
    {
        if (jest_pozar)
        {
            break;
        }

        if (safe_wait_seconds(krok) == -1)
        {
            if (jest_pozar)
            {
                break;
            }
        }

        wykonano += (przyspieszony ? krok * 2.0 : krok);
    }
}

int main(int argc, char *argv[])
{
    srand(getpid() ^ time(NULL));

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

    if (signal(SIGRTMIN, sig_normalnie) == SIG_ERR)
    {
        perror("signal SIGRTMIN failed");
    }

    if (signal(SIGTERM, sig_pozar) == SIG_ERR)
    {
        perror("signal SIGTERM failed");
    }

    Msg msg;

    char buffer[256];
    int dodatkowy_koszt_zaakceptowany = 0;
    int dodatkowy_id_zaakceptowany = -1;

    //Złoszenie obecności w pamięci współdzielonej
    sem_lock(SEM_SHARED);
    shared->stanowiska[id_stanowiska].pid_mechanika = getpid();
    sem_unlock(SEM_SHARED);

    printf("[MECHANIK %d] Stanowisko %d (Marki : %s)\n", getpid(), id_stanowiska, (id_stanowiska == 7) ? "U i Y" : "A, E, I, O, U i Y" );
    snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Stanowisko %d (Marki : %s)", getpid(), id_stanowiska, (id_stanowiska == 7) ? "U i Y" : "A, E, I, O, U i Y" );
    zapisz_log(buffer);

    //Pętla dni pracy
    while (1)
    {
        if (jest_pozar)
        {
            break;
        }

        //Czekanie na otwarcie serwisu
        wait_serwis_otwarty();
        
        if (jest_pozar)
        {
            break;
        }

        printf("[MECHANIK %d] Serwis otwarty, przygotowuję stanowisko %d\n", getpid(), id_stanowiska);
        snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Serwis otwarty, przygotowuję stanowisko %d", getpid(), id_stanowiska);
        zapisz_log(buffer);

        //Reset stanu stanowiska na początek dnia
        sem_lock(SEM_SHARED);
        shared->stanowiska[id_stanowiska].zajete = 0;
        sem_unlock(SEM_SHARED);

        //Pętla obsługi aut w ciągu dnia
        while (1)
        {
            if (jest_pozar)
            {
                break;
            }

            //Sprawdzenie czy serwis jest nadal otwarty
            sem_lock(SEM_SHARED);
            int otwarte = shared->serwis_otwarty;
            sem_unlock(SEM_SHARED);

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
                sem_lock(SEM_SHARED);
                //Sprawdzenie czy są jeszcze auta do obsłużenia
                if (recv_msg(msg_id, &msg, 100 + id_stanowiska, IPC_NOWAIT) != -1)
                {
                    //Obsługa auta przed zamknięciem stanowiska
                    msg.mtype = MSG_REJESTRACJA;
                    msg.samochod.id_stanowiska_roboczego = -1;
                    shared->liczba_oczekujacych_klientow++;

                    printf("[MECHANIK %d] Auto %d wróciło do kolejki oczekujących\n", getpid(), msg.samochod.pid_kierowcy);
                    snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Auto %d wróciło do kolejki oczekujących", getpid(), msg.samochod.pid_kierowcy);
                    zapisz_log(buffer);
                    
                    sem_unlock(SEM_SHARED);

                    if (send_msg(msg_id, &msg) == -1)
                    {
                        perror("[MECHANIK] Błąd odsyłania samochodu do kolejki");
                    }
                    signal_nowa_wiadomosc();
                }
                else
                {
                    sem_unlock(SEM_SHARED);
                }

                printf("[MECHANIK %d] Zamykam stanowisko %d\n", getpid(), id_stanowiska);
                snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Zamykam stanowisko %d", getpid(), id_stanowiska);
                zapisz_log(buffer);

                //Zamknięcie stanowiska
                sem_lock(SEM_SHARED);
                shared->stanowiska[id_stanowiska].pid_mechanika = -1;
                shared->stanowiska[id_stanowiska].zajete = 0;
                sem_unlock(SEM_SHARED);
                exit(0);
            }

            //Oczekiwanie na zlecenie naprawy auta
            if (recv_msg(msg_id, &msg, 100 + id_stanowiska, IPC_NOWAIT) == -1)
            {
                if (errno == ENOMSG || errno == EINTR)
                {
                    if (jest_pozar)
                    {
                        break;
                    }

                    if (zamknij_po && errno != EINTR)
                    {
                        printf("[MECHANIK %d] Zamykam stanowisko %d\n", getpid(), id_stanowiska);
                        snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Zamykam stanowisko %d", getpid(), id_stanowiska);
                        zapisz_log(buffer);

                        sem_lock(SEM_SHARED);
                        shared->stanowiska[id_stanowiska].pid_mechanika = -1;
                        shared->stanowiska[id_stanowiska].zajete = 0;
                        sem_unlock(SEM_SHARED);
                        exit(0);
                    }

                    if (errno == ENOMSG)
                    {
                        wait_nowa_wiadomosc(0);
                    }

                    continue;
                }

                perror("[MECHANIK] Błąd odbierania wiadomości");
                exit(1);
            }

            printf("[MECHANIK %d] Otrzymano zlecenie naprawy auta %d\n", getpid(), msg.samochod.pid_kierowcy);
            snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Otrzymano zlecenie naprawy auta %d", getpid(), msg.samochod.pid_kierowcy);
            zapisz_log(buffer);

            dodatkowy_koszt_zaakceptowany = 0;
            dodatkowy_id_zaakceptowany = -1;
            
            //Rozpoczęcie naprawy auta
            sem_lock(SEM_SHARED);
            shared->stanowiska[id_stanowiska].zajete = 1;
            sem_unlock(SEM_SHARED);

            printf("[MECHANIK %d] Naprawiam auto %d (Marka: %s)\n", getpid(), msg.samochod.pid_kierowcy, msg.samochod.marka);
            snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Naprawiam auto %d (Marka: %s)", getpid(), msg.samochod.pid_kierowcy, msg.samochod.marka);
            zapisz_log(buffer);

            double czas_calkowity = (double)msg.samochod.czas_naprawy;
            int part1 = czas_calkowity / 2.0;

            wykonaj_prace(part1);

            if (jest_pozar)
            {
                //Procedura ewakuacji podczas pożaru
                printf("[MECHANIK %d] Pożar! Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!\n", getpid(), msg.samochod.pid_kierowcy);
                snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Pożar! Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!", getpid(), msg.samochod.pid_kierowcy);
                zapisz_log(buffer);
                
                msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                msg.samochod.ewakuacja = 1;
                msg.samochod.koszt = 0;

                if (send_msg(msg_id, &msg) == -1)
                {
                    perror("[MECHANIK] Błąd wysyłania wiadomości o ewakuacji");
                }
                signal_nowa_wiadomosc();

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
                send_msg(msg_id, &msg);
                signal_nowa_wiadomosc();

                printf("[MECHANIK %d] Zgłoszono dodatkową usterkę do Pracownika Serwisu\n", getpid());
                snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Zgłoszono dodatkową usterkę do Pracownika Serwisu", getpid());
                zapisz_log(buffer);

                Msg odp;

                //Czekanie na decyzję kierowcy
                while (1)
                {
                    if (jest_pozar)
                    {
                        break;
                    }

                    if (recv_msg(msg_id, &odp, 100 + id_stanowiska, IPC_NOWAIT) != -1)
                    {
                        //Sprawdzenie czy to odpowiedź dla tego auta
                        if (odp.samochod.pid_kierowcy == msg.samochod.pid_kierowcy)
                        {
                            msg = odp;
                            break;
                        }
                        else
                        {
                            send_msg(msg_id, &odp);
                            signal_nowa_wiadomosc();
                        }
                    }
                    else
                    {
                        wait_nowa_wiadomosc(0);
                    }
                }

                if (jest_pozar)
                {
                    //Procedura ewakuacji podczas pożaru
                    printf("[MECHANIK %d] Pożar! Nie czekam na decyzję. Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!\n", getpid(), msg.samochod.pid_kierowcy);
                    snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Pożar! Nie czekam na decyzję. Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!", getpid(), msg.samochod.pid_kierowcy);
                    zapisz_log(buffer);
                    
                    msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                    msg.samochod.ewakuacja = 1;
                    msg.samochod.koszt = 0;
                    send_msg(msg_id, &msg);
                    signal_nowa_wiadomosc();

                    sem_lock(SEM_SHARED);
                    shared->stanowiska[id_stanowiska].zajete = 0;
                    sem_unlock(SEM_SHARED);
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
            
            if (jest_pozar)
            {
                printf("[MECHANIK %d] Pożar! Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!\n", getpid(), msg.samochod.pid_kierowcy);
                snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Pożar! Przerywam pracę nad autem %d. Oddaje kluczyki kierowcy!", getpid(), msg.samochod.pid_kierowcy);
                zapisz_log(buffer);

                msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                msg.samochod.ewakuacja = 1;
                msg.samochod.koszt = 0;
                send_msg(msg_id, &msg);
                signal_nowa_wiadomosc();

                sem_lock(SEM_SHARED);
                shared->stanowiska[id_stanowiska].zajete = 0;
                sem_unlock(SEM_SHARED);
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
            
            send_msg(msg_id, &msg);
            signal_nowa_wiadomosc();

            sem_lock(SEM_SHARED);
            shared->stanowiska[id_stanowiska].zajete = 0;
            sem_unlock(SEM_SHARED);

            signal_wolny_mechanik();
        }

        sem_lock(SEM_SHARED);
        shared->stanowiska[id_stanowiska].zajete = 0;
        sem_unlock(SEM_SHARED);

        if (jest_pozar)
        {
            printf("[MECHANIK %d] Uciekam przed pożarem!\n", getpid());
            snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Uciekam przed pożarem!", getpid());
            zapisz_log(buffer);

            break;
        }

        printf("[MECHANIK %d] Czekam na kolejny dzień...\n", getpid());
        snprintf(buffer, sizeof(buffer), "[MECHANIK %d] Czekam na kolejny dzień...", getpid());
        zapisz_log(buffer);
    }
    return 0;
}