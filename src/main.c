#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

//Flaga główna
volatile int running = 1;

void handle_sigterm(int sig)
{
    printf("\n[MAIN] Zamknięcie serwisu\n");
    running = 0;
}

int main()
{
    //Obsługa sygnałów
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigterm);

    //Zapobiegamy powstawaniu zombie
    signal(SIGCHLD, SIG_IGN);

    srand(time(NULL));

    init_ipc(1);

    char arg_buff[10];

    //Kasjer
    if (fork() == 0)
    {
        execl("./kasjer", "kasjer", NULL);
        perror("execl kasjer failed");
        exit(1);
    }

    //Pracownik serwisu
    for (int i = 0; i < 3; i++)
    {
        if (fork() == 0)
        {
            sprintf(arg_buff, "%d", i);
            execl("./pracownik_serwisu", "pracownik_serwisu", arg_buff, NULL);
            perror("execl pracownik_serwisu failed");
            exit(1);
        }
    }

    //Mechanicy
    for (int i = 0; i < MAX_STANOWISK; i++)
    {
        if (fork() == 0)
        {
            sprintf(arg_buff, "%d", i);
            execl("./mechanik", "mechanik", arg_buff, NULL);
            perror("execl mechanik failed");
            exit(1);
        }
    }

    sleep(1);

    //Kierownik
    if (fork() == 0)
    {
        execl("./kierownik", "kierownik", NULL);
        perror("execl kierownik failed");
        exit(1);
    }

    printf("[MAIN] System wystartował. Generowanie klientów...\n");

    //Kierowcy
    while (running)
    {
        usleep((rand() % 2000) * 1000);

        if (fork() == 0)
        {
            execl("./kierowca", "kierowca", NULL);
            perror("execl kierowca failed");
            exit(1);
        }
    }

    kill(0, SIGTERM);

    cleanup_ipc();
    return 0;
}