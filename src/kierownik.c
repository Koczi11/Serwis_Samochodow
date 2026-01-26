#define _DEFAULT_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define SEC_PER_H 5.0

int main()
{
    //Dołączenie do IPC
    init_ipc(0);

    //Kierownik ignoruje sygnał pożaru, który sam wysyła do grupy
    if (signal(SIGUSR1, SIG_IGN) == SIG_ERR)
    {
        perror("signal SIGUSR1 ignore failed");
    }

    srand(time(NULL));
    char buffer[256];

    printf("[KIEROWNIK] Uruchomiony. Zaczynamy dzień!\n");
    snprintf(buffer, sizeof(buffer), "[KIEROWNIK] Uruchomiony. Zaczynamy dzień!");
    zapisz_log(buffer);

    sem_lock(SEM_SHARED);
    shared->aktualna_godzina = 6;
    shared->pozar = 0;
    shared->serwis_otwarty = 0;
    shared->liczba_oczekujacych_klientow = 0;
    sem_unlock(SEM_SHARED);

    while (1)
    {
        if (safe_wait_seconds(SEC_PER_H) == -1)
        {
            //Przerwanie
        }
        
        sem_lock(SEM_SHARED);

        shared->aktualna_godzina++;
        if (shared->aktualna_godzina > 23)
        {
            shared->aktualna_godzina = 0;
        }

        int godzina = shared->aktualna_godzina;
        int pozar = shared->pozar;

        //Reset pożaru o 5:00
        if (godzina == 5)
        {
            if (pozar || shared->reset_po_pozarze)
            {
                shared->auta_w_serwisie = 0;
                shared->liczba_oczekujacych_klientow = 0;
                shared->reset_po_pozarze = 0;
                shared->pozar = 0;

                sem_unlock(SEM_SHARED);
                drain_msg_queue();
                clear_wakeup_sems();
                sem_lock(SEM_SHARED);
            }

            pozar = 0;
        }

        //Decyzja o otwarciu/zamknięciu serwisu
        if (!pozar)
        {
            if (godzina >= GODZINA_OTWARCIA && godzina < GODZINA_ZAMKNIECIA)
            {
                if (!shared->serwis_otwarty)
                {
                    printf("[KIEROWNIK] Godzina %d:00. Otwieram serwis\n", godzina);
                    snprintf(buffer, sizeof(buffer), "[KIEROWNIK] Godzina %d:00. Otwieram serwis", godzina);
                    zapisz_log(buffer);

                    shared->serwis_otwarty = 1;

                    sem_unlock(SEM_SHARED);
                    signal_serwis_otwarty();
                    sem_lock(SEM_SHARED);
                }
            }
            else
            {
                if (shared->serwis_otwarty)
                {
                    printf("[KIEROWNIK] Godzina %d:00. Zamykam serwis\n", godzina);
                    snprintf(buffer, sizeof(buffer), "[KIEROWNIK] Godzina %d:00. Zamykam serwis", godzina);
                    zapisz_log(buffer);

                    shared->serwis_otwarty = 0;
                }
            }
        }
        sem_unlock(SEM_SHARED);

        //Informacje o stanie serwisu
        if (shared->serwis_otwarty == 0)
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
        if (shared->serwis_otwarty && !pozar)
        {
            int stanowisko = rand() % MAX_STANOWISK;

            //Pobieramy PID mechanika z wylosowanego stanowiska
            sem_lock(SEM_SHARED);
            pid_t pid = shared->stanowiska[stanowisko].pid_mechanika;
            sem_unlock(SEM_SHARED);

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
                        
                        sem_lock(SEM_SHARED);
                        shared->serwis_otwarty = 0;
                        shared->pozar = 1;
                        shared->reset_po_pozarze = 1;
                        sem_unlock(SEM_SHARED);

                        //Czyścimy zaległe komunikaty i semafory
                        drain_msg_queue();
                        clear_wakeup_sems();

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
    return 0;
}