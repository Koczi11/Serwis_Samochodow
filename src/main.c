#define _DEFAULT_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

//Flaga sterująca pętlą główną
volatile sig_atomic_t running = 1;

//Obsługa sygnału zakończenia procesu potomnego
void handle_sigchld(int sig)
{
    (void) sig;

    //Zapisywanie errno, aby nie zostało nadpisane
    int saved_errno = errno;

    //Czyszczenie zakończonych procesów potomnych
    while (waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

//Obsługa sygnału zamknięcia
void handle_sigterm(int sig)
{
    (void) sig;

    const char *msg = "\n[MAIN] Zamknięcie serwisu\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    running = 0;
}

int main()
{
    //Konfiguracja obsługi sygnałów
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigterm);

    //Zapobieganie powstawaniu procesów zombie
    signal(SIGCHLD, handle_sigchld);

    srand(time(NULL));

    //Tworzenie zasobów IPC
    //1 - tworzymy i zerujemy pamięć
    init_ipc(1);

    //Bufor do przekazywania argumentów do procesów potomnych
    char arg_buff[10];

    //Uruchamianie personelu serwisu
    
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

    //sleep(1);

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
        //usleep(2000000 + (rand() % 3000000));
        safe_wait_seconds(2 + (rand() % 4));

        if (!running)
            break;

        if (fork() == 0)
        {
            execl("./kierowca", "kierowca", NULL);
            perror("execl kierowca failed");
            exit(1);
        }
    }

    printf("[MAIN] Kończenie pracy...\n");

    //Ignorowanie SIGTERM w main, aby kill(0) nas nie zabił
    signal(SIGTERM, SIG_IGN);

    //Zamykanie procesów potomnych
    kill(0, SIGTERM);

    //Czekanie na zakończenie procesów stałych
    while(wait(NULL) > 0);

    //Czyszczenie zasobów IPC
    cleanup_ipc();

    return 0;
}