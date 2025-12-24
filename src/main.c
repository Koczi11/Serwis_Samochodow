#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

void handle_sigint(int sig)
{
    printf("\n[MAIN] SIGINT - sprzątanie IPC\n");
    cleanup_ipc();
    exit(0);
}

int main()
{
    init_ipc();

    //Pracownik serwisu
    if (fork() == 0)
    {
        execl("./pracownik_serwisu", "pracownik_serwisu", NULL);
    }

    //Mechanicy
    for (int i = 0; i < MAX_STANOWISK; i++)
    {
        if (fork() == 0)
        {
            execl("./mechanik", "mechanik", NULL);
        }
    }

    sleep(1);

    //Kierowcy
    for (int i = 0; i < 5; i++)
    {
        if (fork() == 0)
        {
            execl("./kierowca", "kierowca", NULL);
        }

        sleep(1);
    }

    pause();
    cleanup_ipc();
    return 0;
}