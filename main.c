#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

void handle_sigint(int sig)
{
    printf("\n[MAIN] SIGINT - sprzątanie IPC\n");
    cleanup_ipc();
    exit(0);
}

int main()
{
    signal(SIGINT, handle_sigint);

    printf("[MAIN] Start inicjalizacji serwisu\n");
    init_ipc();

    printf("[MAIN] Serwis gotowy\n");
    printf("[MAIN] Naciśnij Ctrl+C aby zakończyć\n");

    while (1)
    {
        pause();
    }

    return 0;
}