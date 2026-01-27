#define _GNU_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdlib.h>

//Flaga ewakuacji
volatile sig_atomic_t ewakuacja = 0;

//Obsługa sygnału pożaru
void handle_pozar(int sig)
{
    (void)sig;
    ewakuacja = 1;
}

//Funkcja sprawdzająca, czy są aktywni mechanicy
int aktywni_mechanicy()
{
    int aktywni = 0;
    sem_lock(SEM_STANOWISKA);
    for (int i = 0; i < MAX_STANOWISK; i++)
    {
        if (shared->stanowiska[i].zajete)
        {
            aktywni = 1;
            break;
        }
    }
    sem_unlock(SEM_STANOWISKA);
    if (!aktywni)
    {
        sem_lock(SEM_LICZNIKI);
        if (shared->liczba_oczekujacych_klientow > 0)
        {
            aktywni = 1;
        }
        sem_unlock(SEM_LICZNIKI);
    }
    return aktywni;
}

int main()
{
    //Dołączanie do IPC
    init_ipc(0);
    join_service_group();

    //Rejestracja handlera pożaru
    struct sigaction sa;
    sa.sa_handler = handle_pozar;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1)
    {
        perror("sigaction SIGUSR1 failed");
    }

    Msg msg;
    char buffer[256];

    //Pętla dni pracy
    while (1)
    {
        if (ewakuacja)
        {
            ewakuacja = 0;
        }

        while (1)
        {
            int r = recv_msg(msg_id_kasjer, &msg, MSG_CTRL_OPEN_KASJER, 0);
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
                printf("[KASJER] Otrzymano sygnał pożaru! Uciekam!\n");
                snprintf(buffer, sizeof(buffer), "[KASJER] Otrzymano sygnał pożaru! Uciekam!");
                zapisz_log(buffer);
                ewakuacja = 0;
                continue;
            }
        }

        if (ewakuacja)
        {
            printf("[KASJER] Otrzymano sygnał pożaru! Uciekam!\n");
            snprintf(buffer, sizeof(buffer), "[KASJER] Otrzymano sygnał pożaru! Uciekam!");
            zapisz_log(buffer);
            ewakuacja = 0;
            continue;
        }

        printf("[KASJER] Kasa otwarta\n");
        snprintf(buffer, sizeof(buffer), "[KASJER] Kasa otwarta");
        zapisz_log(buffer);

        int dzienny_utarg = 0;
        zapisz_raport("[KASJER] Kasa otwarta. Rozpoczynamy nowy dzień.");

        //Pętla obsługi klientów
        while (1)
        {
            if (ewakuacja)
            {
                printf("[KASJER] Otrzymano sygnał pożaru! Uciekam!\n");
                snprintf(buffer, sizeof(buffer), "[KASJER] Otrzymano sygnał pożaru! Uciekam!");
                zapisz_log(buffer);

                snprintf(buffer, sizeof(buffer), "[KASJER] Dzień przerwany przez pożar. Zebrany utarg do momentu ewakuacji: %d PLN", dzienny_utarg);
                zapisz_raport(buffer);

                break;
            }

            sem_lock(SEM_STATUS);
            int otwarte = shared->serwis_otwarty;
            sem_unlock(SEM_STATUS);

            //Obsługa płatności klientów
            int rcv = recv_msg(msg_id_kasjer, &msg, MSG_KASA, IPC_NOWAIT);
            if (rcv == -2)
            {
                exit(0);
            }
            if (rcv == 0)
            {
                printf("[KASJER] Klient %d płaci %d PLN\n", msg.samochod.pid_kierowcy, msg.samochod.koszt);
                snprintf(buffer, sizeof(buffer), "[KASJER] Klient %d płaci %d PLN", msg.samochod.pid_kierowcy, msg.samochod.koszt);
                zapisz_log(buffer);
                //safe_wait_seconds(2);

                //Aktualizacja dziennego utargu
                dzienny_utarg += msg.samochod.koszt;

                //Zapisujemy raport o płatności
                snprintf(buffer, sizeof(buffer), "[KASJER] Pobrano opłatę %d PLN od kierowcy %d", msg.samochod.koszt, msg.samochod.pid_kierowcy);
                zapisz_raport(buffer);

                //Odsyłamy potwierdzenie płatności do kierowcy
                msg.mtype = MSG_POTWIERDZENIE_PLATNOSCI_PID(msg.samochod.pid_kierowcy);
                msg.samochod.dodatkowa_usterka = 0;
                
                if(send_msg(msg_id_kasjer, &msg) == -1)
                {
                    perror("[KASJER] Błąd wysłania wiadomości");
                }
                else
                {
                    printf("[KASJER] Płatność zakończona dla klienta %d\n", msg.samochod.pid_kierowcy);
                    snprintf(buffer, sizeof(buffer), "[KASJER] Płatność zakończona dla klienta %d", msg.samochod.pid_kierowcy);
                    zapisz_log(buffer);

                    //Dekrementacja liczniy aut w serwisie
                    sem_lock(SEM_LICZNIKI);
                    if (shared->auta_w_serwisie > 0)
                    {
                        shared->auta_w_serwisie--;
                    }
                    sem_unlock(SEM_LICZNIKI);
                }

                //Przejdź do obsługi następnego klienta
                continue;
            }
            else
            {
                if (errno == EINTR && ewakuacja)
                {
                    printf("[KASJER] Otrzymano sygnał pożaru! Uciekam!\n");
                    snprintf(buffer, sizeof(buffer), "[KASJER] Otrzymano sygnał pożaru! Uciekam!");
                    zapisz_log(buffer);

                    snprintf(buffer, sizeof(buffer), "[KASJER] Dzień przerwany przez pożar. Zebrany utarg do momentu ewakuacji: %d PLN", dzienny_utarg);
                    zapisz_raport(buffer);

                    break;
                }
            }

            //Procedura zamknięcia kasy, jeśli serwis jest zamknięty i nie ma klientów
            if (!otwarte)
            {
                int auta_zostaly = 0;
                sem_lock(SEM_LICZNIKI);
                auta_zostaly = shared->auta_w_serwisie;
                sem_unlock(SEM_LICZNIKI);

                if (!aktywni_mechanicy() && auta_zostaly == 0)
                {
                    //Ostanie sprawdzenie wiadomości
                    if (recv_msg(msg_id_kasjer, &msg, MSG_KASA, IPC_NOWAIT) == -1)
                    {
                        printf("[KASJER] Kasa zamknięta, brak klientów\n");
                        snprintf(buffer, sizeof(buffer), "[KASJER] Kasa zamknięta o godzinie %d, brak klientów", shared->aktualna_godzina);
                        zapisz_log(buffer);
                        
                        //Zapisujemy raport o zamknięciu kasy
                        snprintf(buffer, sizeof(buffer), "[KASJER] Kasa zamknięta. Dzienny utarg: %d PLN", dzienny_utarg);
                        zapisz_raport(buffer);
                        printf("%s\n", buffer);
                        
                        break;
                    }
                }
            }

            //Nieblokujące oczekiwanie na płatność
            int r = recv_msg(msg_id_kasjer, &msg, MSG_KASA, IPC_NOWAIT);
            if (r == -2)
            {
                exit(0);
            }
            if (r != -1)
            {
                printf("[KASJER] Klient %d płaci %d PLN\n", msg.samochod.pid_kierowcy, msg.samochod.koszt);
                snprintf(buffer, sizeof(buffer), "[KASJER] Klient %d płaci %d PLN", msg.samochod.pid_kierowcy, msg.samochod.koszt);
                zapisz_log(buffer);

                dzienny_utarg += msg.samochod.koszt;

                snprintf(buffer, sizeof(buffer), "[KASJER] Pobrano opłatę %d PLN od kierowcy %d", msg.samochod.koszt, msg.samochod.pid_kierowcy);
                zapisz_raport(buffer);

                msg.mtype = MSG_POTWIERDZENIE_PLATNOSCI_PID(msg.samochod.pid_kierowcy);
                msg.samochod.dodatkowa_usterka = 0;

                if(send_msg(msg_id_kasjer, &msg) == -1)
                {
                    perror("[KASJER] Błąd wysłania wiadomości");
                }
                else
                {
                    printf("[KASJER] Płatność zakończona dla klienta %d\n", msg.samochod.pid_kierowcy);
                    snprintf(buffer, sizeof(buffer), "[KASJER] Płatność zakończona dla klienta %d", msg.samochod.pid_kierowcy);
                    zapisz_log(buffer);

                    sem_lock(SEM_LICZNIKI);
                    if (shared->auta_w_serwisie > 0)
                    {
                        shared->auta_w_serwisie--;
                    }
                    sem_unlock(SEM_LICZNIKI);
                }

                continue;
            }

            safe_wait_seconds(0.2);
        }
    }
    return 0;
}