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

    printf("[KIEROWNIK] Uruchomiony\n");

    while (1)
    {
        sleep(5);

        int stanowisko = rand() % MAX_STANOWISK;

        sem_lock(SEM_SHARED);
        pid_t pid = shared->stanowiska[stanowisko].pid_mechanika;
        int zajete = shared->stanowiska[stanowisko].zajete;
        sem_unlock(SEM_SHARED);

        if (pid <= 0)
            continue;

        int los = rand () % 100;
        int akcja = -1;

        if (los < 40)
            akcja = 1;          //Przyspieszenie (40% szans)
        else if (los < 80)
            akcja = 2;          //Normalna praca (40% szans)
        else if (los < 95)
            akcja = 0;          //Zamknięcie (15% szans)
        else
            akcja = 3;          //Pożar (5% szans)

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
                kill(0, SIGTERM);
                exit(0);
                break;
        }
    }
    return 0;
}