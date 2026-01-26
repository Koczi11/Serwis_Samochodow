#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#define DEFAULT_KIEROWCY 50000

int main(int argc, char *argv[])
{
    int liczba = DEFAULT_KIEROWCY;

    if (argc >= 2)
    {
        int tmp = atoi(argv[1]);
        if (tmp > 0)
        {
            liczba = tmp;
        }
    }

    //Automatyczne sprzątanie po procesach potomnych, jeśli generator działa dłużej
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
    {
        perror("signal SIGCHLD failed");
    }

    printf("[GENERATOR] Start generowania %d kierowców\n", liczba);

    for (int i = 0; i < liczba; i++)
    {
        while (1)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                execl("./kierowca", "kierowca", NULL);
                perror("execl kierowca failed");
                _exit(1);
            }
            else if (pid > 0)
            {
                break;
            }
            else
            {
                if (errno == EAGAIN || errno == ENOMEM)
                {
                    usleep(1000);
                    continue;
                }

                fprintf(stderr, "[GENERATOR] fork failed: %s\n", strerror(errno));
                return 1;
            }
        }
    }

    printf("[GENERATOR] Zakończono uruchamianie kierowców\n");
    return 0;
}
