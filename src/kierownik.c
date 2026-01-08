#define _DEFAULT_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

int main()
{
    init_ipc(0);

    srand(time(NULL));

    printf("[KIEROWNIK] Uruchomiony. Zaczynamy dzień!\n");

    sem_lock(SEM_SHARED);
    shared->aktualna_godzina = 6;
    shared->pozar = 0;
    shared->serwis_otwarty = 0;
    sem_unlock(SEM_SHARED);

    while (1)
    {
        sleep(5);

        sem_lock(SEM_SHARED);
        shared->aktualna_godzina++;
        if (shared->aktualna_godzina > 23)
        {
            shared->aktualna_godzina = 0;
        }

        int godzina = shared->aktualna_godzina;
        int pozar = shared->pozar;
        int otwarte = shared->serwis_otwarty;

        if (godzina == 5)
        {
            shared->pozar = 0;
            pozar = 0;
        }

        if (godzina >= GODZINA_OTWARCIA && godzina < GODZINA_ZAMKNIECIA)
        {
            if (!otwarte && !pozar)
            {
                printf("[KIEROWNIK] Godzina %d:00. Otwieram serwis\n", godzina);
                shared->serwis_otwarty = 1;
                otwarte = 1;
            }
        }
        else
        {
            if (otwarte)
            {
                printf("[KIEROWNIK] Godzina %d:00. Zamykam serwis\n", godzina);
                shared->serwis_otwarty = 0;
                otwarte = 0;
            }
        }
        sem_unlock(SEM_SHARED);

        if (!otwarte)
        {
            if (pozar)
            {
                printf("[KIEROWNIK] Godzina %d:00. Trwa sprzątanie po pożarze\n", godzina); 
            }
            else
            {
                printf("[KIEROWNIK] Godzina %d:00. Serwis jest zamknięty\n", godzina); 
            }
            continue;
        }

        int stanowisko = rand() % MAX_STANOWISK;

        sem_lock(SEM_SHARED);
        pid_t pid = shared->stanowiska[stanowisko].pid_mechanika;
        sem_unlock(SEM_SHARED);

        if (pid <= 0)
            continue;

        int los = rand () % 100;

        if (los < 10)
        {
            int akcja = rand() % 4;

            switch (akcja)
            {
                case 0:
                    printf("[KIEROWNIK] Zamknięcie stanowiska %d\n", stanowisko);
                    if (kill(pid, SIGUSR1) == -1) 
                    {
                        perror("[KIEROWNIK] Błąd wysłania SIGUSR1");
                    }
                    break;

                case 1:
                    printf("[KIEROWNIK] Przyspieszenie stanowiska %d\n", stanowisko);
                    if (kill(pid, SIGUSR2) == -1) 
                    {
                        perror("[KIEROWNIK] Błąd wysłania SIGUSR2");
                    }
                    break;

                case 2:
                    printf("[KIEROWNIK] Normalna praca stanowiska %d\n", stanowisko);
                    if (kill(pid, SIGCONT) == -1) 
                    {
                        perror("[KIEROWNIK] Błąd wysłania SIGCONT");
                    }
                    break;

                case 3:
                    printf("[KIEROWNIK] POŻAR!\n");
                    sem_lock(SEM_SHARED);
                    shared->serwis_otwarty = 0;
                    shared->pozar = 1;
                    sem_unlock(SEM_SHARED);
                    break;
            }
        }
    }
    return 0;
}