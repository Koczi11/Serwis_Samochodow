#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

int main()
{
    srand(time(NULL));

    printf("[KIEROWNIK] Uruchomiony\n");

    while (1)
    {
        sleep(5);

        int stanowisko = rand() % MAX_STANOWISK;

        sem_lock(SEM_SHARED);
        pid_t pid = shared->stanowiska[stanowisko].pid_mechanika;
        sem_unlock(SEM_SHARED);

        if (pid <= 0)
            continue;

        int akcja = rand() % 4;

        switch (akcja)
        {
            case 0:
                printf("[KIEROWNIK] Zamknięcie stanowiska %d\n", stanowisko);
                kill(pid, SIGUSR1);
                break;

            case 1:
                printf("[KIEROWNIK] Przyspieszenie stanowiska %d\n", stanowisko);
                kill(pid, SIGUSR2);
                break;

            case 2:
                printf("[KIEROWNIK] Normalna praca stanowiska %d\n", stanowisko);
                kill(pid, SIGCONT);
                break;

            case 3:
                printf("[KIEROWNIK] POŻAR!\n");
                kill(0, SIGTERM);
                break;
        }
    }
}
