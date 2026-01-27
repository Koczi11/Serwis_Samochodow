#define _DEFAULT_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define SEC_PER_H 5.0

//Flaga sterująca pętlą główną
static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t shutdown_requested = 0;

//Obsługa sygnału zamknięcia
static void handle_sigterm(int sig)
{
    (void) sig;

    const char *msg = "\n[KIEROWNIK] Zamknięcie serwisu\n";
    if (write(STDOUT_FILENO, msg, strlen(msg)) == -1)
    {
        perror("write failed");
    }
    running = 0;
    shutdown_requested = 1;
}

static void shutdown_serwis()
{
    //Zamykamy serwis i budzimy procesy czekające
    sem_lock(SEM_STATUS);
    shared->serwis_otwarty = 0;
    sem_unlock(SEM_STATUS);

    //Ignorujemy SIGTERM/SIGINT dla kierownika
    if (signal(SIGTERM, SIG_IGN) == SIG_ERR)
    {
        perror("signal SIGTERM ignore failed");
    }
    if (signal(SIGINT, SIG_IGN) == SIG_ERR)
    {
        perror("signal SIGINT ignore failed");
    }

    //Wysyłamy SIGTERM do całej grupy procesów
    if (kill(0, SIGTERM) == -1)
    {
        perror("kill SIGTERM failed");
    }

    int status;
    while (wait(&status) > 0)
    {
        //Czekamy na zakończenie wszystkich procesów potomnych
    }
}

int main()
{
    //Konfiguracja obsługi sygnałów (bez SA_RESTART)
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("sigaction SIGTERM failed");
        exit(1);
    }
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction SIGINT failed");
        exit(1);
    }

    //Dołączenie do IPC (kierownik tworzy zasoby)
    init_ipc(1);

    if (setpgid(0, 0) == -1)
    {
        perror("setpgid kierownik failed");
    }

    sem_lock(SEM_STATUS);
    shared->pid_kierownik = getpid();
    sem_unlock(SEM_STATUS);

    //Kierownik ignoruje sygnał pożaru, który sam wysyła do grupy
    struct sigaction sa_ign;
    memset(&sa_ign, 0, sizeof(sa_ign));
    sa_ign.sa_handler = SIG_IGN;
    sigemptyset(&sa_ign.sa_mask);
    sa_ign.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa_ign, NULL) == -1)
    {
        perror("sigaction SIGUSR1 ignore failed");
    }

    srand(time(NULL));
    char buffer[256];

    printf("[KIEROWNIK] Uruchomiony. Zaczynamy dzień!\n");
    snprintf(buffer, sizeof(buffer), "[KIEROWNIK] Uruchomiony. Zaczynamy dzień!");
    zapisz_log(buffer);

    sem_lock(SEM_STATUS);
    shared->aktualna_godzina = 6;
    shared->pozar = 0;
    shared->serwis_otwarty = 0;
    sem_unlock(SEM_STATUS);

    sem_lock(SEM_LICZNIKI);
    shared->liczba_oczekujacych_klientow = 0;
    shared->liczba_czekajacych_na_otwarcie = 0;
    sem_unlock(SEM_LICZNIKI);

    while (running)
    {
        if (safe_wait_seconds(SEC_PER_H) == -1)
        {
            if (!running)
            {
                break;
            }
        }
        
        sem_lock(SEM_STATUS);

        shared->aktualna_godzina++;
        if (shared->aktualna_godzina > 23)
        {
            shared->aktualna_godzina = 0;
        }

        int godzina = shared->aktualna_godzina;
        int pozar = shared->pozar;
        sem_unlock(SEM_STATUS);

        //Reset pożaru o 5:00
        if (godzina == 5)
        {
            sem_lock(SEM_STATUS);
            int reset = shared->reset_po_pozarze;
            sem_unlock(SEM_STATUS);

            if (pozar || reset)
            {
                sem_lock(SEM_LICZNIKI);
                shared->auta_w_serwisie = 0;
                shared->liczba_oczekujacych_klientow = 0;
                shared->liczba_czekajacych_na_otwarcie = 0;
                sem_unlock(SEM_LICZNIKI);

                sem_lock(SEM_STATUS);
                shared->reset_po_pozarze = 0;
                shared->pozar = 0;
                sem_unlock(SEM_STATUS);

            }

            pozar = 0;
        }

        //Decyzja o otwarciu/zamknięciu serwisu
        if (!pozar)
        {
            if (godzina >= GODZINA_OTWARCIA && godzina < GODZINA_ZAMKNIECIA)
            {
                sem_lock(SEM_STATUS);
                int otwarty = shared->serwis_otwarty;
                sem_unlock(SEM_STATUS);

                if (!otwarty)
                {
                    printf("[KIEROWNIK] Godzina %d:00. Otwieram serwis\n", godzina);
                    snprintf(buffer, sizeof(buffer), "[KIEROWNIK] Godzina %d:00. Otwieram serwis", godzina);
                    zapisz_log(buffer);

                    sem_lock(SEM_STATUS);
                    shared->serwis_otwarty = 1;
                    sem_unlock(SEM_STATUS);

                    sem_lock(SEM_LICZNIKI);
                    shared->liczba_czekajacych_na_otwarcie = 0;
                    sem_unlock(SEM_LICZNIKI);

                    Msg ctrl;

                    ctrl.mtype = MSG_CTRL_OPEN_PRACOWNIK;
                    for (int i = 0; i < LICZBA_PRACOWNIKOW; i++)
                    {
                        send_msg(msg_id_kierowca, &ctrl);
                    }

                    ctrl.mtype = MSG_CTRL_OPEN_MECHANIK;
                    for (int i = 0; i < MAX_STANOWISK; i++)
                    {
                        send_msg(msg_id_mechanik, &ctrl);
                    }

                    ctrl.mtype = MSG_CTRL_OPEN_KASJER;
                    send_msg(msg_id_kasjer, &ctrl);

                }
            }
            else
            {
                sem_lock(SEM_STATUS);
                int otwarty = shared->serwis_otwarty;
                if (otwarty)
                {
                    printf("[KIEROWNIK] Godzina %d:00. Zamykam serwis\n", godzina);
                    snprintf(buffer, sizeof(buffer), "[KIEROWNIK] Godzina %d:00. Zamykam serwis", godzina);
                    zapisz_log(buffer);

                    shared->serwis_otwarty = 0;
                }
                sem_unlock(SEM_STATUS);
            }
        }

        //Informacje o stanie serwisu
        sem_lock(SEM_STATUS);
        int otwarty_info = shared->serwis_otwarty;
        sem_unlock(SEM_STATUS);

        if (otwarty_info == 0)
        {
            if (pozar)
            {
                printf("[KIEROWNIK] Godzina %d:00. Trwa sprzątanie po pożarze\n", godzina);
                snprintf(buffer, sizeof(buffer), "[KIEROWNIK] Godzina %d:00. Trwa sprzątanie po pożarze", godzina);
                zapisz_log(buffer);
            }
            else
            {
                printf("[KIEROWNIK] Godzina %d:00. Serwis jest zamknięty\n", godzina); 
                snprintf(buffer, sizeof(buffer), "[KIEROWNIK] Godzina %d:00. Serwis jest zamknięty", godzina);
                zapisz_log(buffer);
            }
            continue;
        }

        //Losowe zdarzenia
        //Kierownik losowo wpływa na pracę mechaników
        if (otwarty_info && !pozar)
        {
            int stanowisko = rand() % MAX_STANOWISK;

            //Pobieramy PID mechanika z wylosowanego stanowiska
            sem_lock(SEM_STANOWISKA);
            pid_t pid = shared->stanowiska[stanowisko].pid_mechanika;
            sem_unlock(SEM_STANOWISKA);

            //10% szansy na zdarzenie
            int los = rand () % 10;
            if (pid > 0 && los < 10)
            {
                int akcja = rand() % 4;

                switch (akcja)
                {
                    //Sygnał 1: Zamknięcie stanowiska
                    case 0:
                        printf("[KIEROWNIK] Zamknięcie stanowiska %d\n", stanowisko);
                        snprintf(buffer, sizeof(buffer), "[KIEROWNIK] Zamknięcie stanowiska %d", stanowisko);
                        zapisz_log(buffer);

                        if (kill(pid, SIGRTMIN) == -1) 
                        {
                            perror("[KIEROWNIK] Błąd wysłania SIGRTMIN");
                        }
                        break;

                    //Sygnał 2: Przyspieszenie pracy stanowiska
                    case 1:
                        printf("[KIEROWNIK] Przyspieszenie stanowiska %d\n", stanowisko);
                        snprintf(buffer, sizeof(buffer), "[KIEROWNIK] Przyspieszenie stanowiska %d", stanowisko);
                        zapisz_log(buffer);

                        if (kill(pid, SIGRTMIN + 1) == -1) 
                        {
                            perror("[KIEROWNIK] Błąd wysłania SIGRTMIN+1");
                        }
                        break;

                    //Sygnał 3: Wznowienie normalnej pracy stanowiska
                    case 2:
                        printf("[KIEROWNIK] Normalna praca stanowiska %d\n", stanowisko);
                        snprintf(buffer, sizeof(buffer), "[KIEROWNIK] Normalna praca stanowiska %d", stanowisko);
                        zapisz_log(buffer);

                        if (kill(pid, SIGRTMIN + 2) == -1) 
                        {
                            perror("[KIEROWNIK] Błąd wysłania SIGRTMIN+2");
                        }
                        break;
                        
                    //Sygnał 4: Pożar w serwisie
                    case 3:
                        printf("[KIEROWNIK] POŻAR! Zarządzam ewakuację całej grupy!\n");
                        snprintf(buffer, sizeof(buffer), "[KIEROWNIK] POŻAR! Zarządzam ewakuację całej grupy!");
                        zapisz_log(buffer);
                        
                        sem_lock(SEM_STATUS);
                        shared->serwis_otwarty = 0;
                        shared->pozar = 1;
                        shared->reset_po_pozarze = 1;
                        sem_unlock(SEM_STATUS);

                        //Wyślij sygnał pożaru do całej grupy procesów
                        if (kill(0, SIGUSR1) == -1)
                        {
                            perror("[KIEROWNIK] Błąd wysyłania sygnału pożaru");
                        }
                        break;
                }
            }
        }
    }

    if (shutdown_requested)
    {
        shutdown_serwis();
    }

    //Czyszczenie zasobów IPC
    cleanup_ipc();
    return 0;
}