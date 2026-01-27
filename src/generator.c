#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdatomic.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "serwis_ipc.h"

#define DEFAULT_KIEROWCY 5000
#define DEFAULT_MAX_ACTIVE 1000

//Flaga sterująca wątkiem reaper i licznik aktywnych dzieci
static volatile sig_atomic_t running = 1;
static atomic_int active_children = 0;
static pthread_mutex_t reap_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t reap_cv = PTHREAD_COND_INITIALIZER;

static void handle_sigterm(int sig)
{
    (void)sig;
    running = 0;
}

static int ipc_available()
{
    key_t key_shm = ftok(".", 'S');
    if (key_shm == -1)
    {
        return 0;
    }

    int id = shmget(key_shm, sizeof(SharedData), 0600);
    if (id == -1)
    {
        if (errno == ENOENT || errno == EIDRM)
        {
            return 0;
        }
        return 0;
    }

    return 1;
}

static void *zombie_reaper(void *arg)
{
    (void)arg;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);

    while (running)
    {
        int sig = 0;
        if (sigwait(&set, &sig) == 0 && sig == SIGCHLD)
        {
            int status = 0;
            while (waitpid(-1, &status, WNOHANG) > 0)
            {
                // Sprzątnięto proces potomny
                atomic_fetch_sub(&active_children, 1);
                pthread_mutex_lock(&reap_mutex);
                pthread_cond_broadcast(&reap_cv);
                pthread_mutex_unlock(&reap_mutex);
            }
        }
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int liczba = DEFAULT_KIEROWCY;
    int max_active = DEFAULT_MAX_ACTIVE;

    char buffer[256];

    if (argc >= 2)
    {
        int tmp = atoi(argv[1]);
        if (tmp > 0)
        {
            liczba = tmp;
        }
    }

    if (argc >= 3)
    {
        int tmp = atoi(argv[2]);
        if (tmp > 0)
        {
            max_active = tmp;
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction SIGINT failed");
        return 1;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("sigaction SIGTERM failed");
        return 1;
    }

    //Blokujemy SIGCHLD w wątku głównym (obsłuży go wątek reaper)
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0)
    {
        perror("pthread_sigmask SIGCHLD failed");
        return 1;
    }

    //Wątek do sprzątania procesów zombie
    pthread_t reaper_thread;
    if (pthread_create(&reaper_thread, NULL, zombie_reaper, NULL) == 0)
    {
        pthread_detach(reaper_thread);
    }

    printf("[GENERATOR] Start generowania %d kierowców\n", liczba);
    snprintf(buffer, sizeof(buffer), "[GENERATOR] Start generowania %d kierowców", liczba);
    zapisz_log(buffer);

    for (int i = 0; i < liczba; i++)
    {
        if (!running)
        {
            break;
        }

        if (!ipc_available())
        {
            fprintf(stderr, "[GENERATOR] IPC nie istnieje. Konczę tworzenie kierowców.\n");
            break;
        }

        pthread_mutex_lock(&reap_mutex);
        while (running && atomic_load(&active_children) >= max_active)
        {
            pthread_cond_wait(&reap_cv, &reap_mutex);
        }
        pthread_mutex_unlock(&reap_mutex);

        while (1)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                sigset_t empty;
                sigemptyset(&empty);
                pthread_sigmask(SIG_SETMASK, &empty, NULL);

                execl("./kierowca", "kierowca", NULL);
                perror("execl kierowca failed");
                _exit(1);
            }
            else if (pid > 0)
            {
                atomic_fetch_add(&active_children, 1);
                break;
            }
            else
            {
                if (errno == EAGAIN || errno == ENOMEM)
                {
                    fprintf(stderr, "[GENERATOR] Brak zasobów do fork (EAGAIN/ENOMEM). Przerywam.\n");
                    return 1;
                }

                fprintf(stderr, "[GENERATOR] fork failed: %s\n", strerror(errno));
                return 1;
            }
        }
    }

    printf("[GENERATOR] Zakończono uruchamianie kierowców\n");
    snprintf(buffer, sizeof(buffer), "[GENERATOR] Zakończono uruchamianie kierowców");;
    zapisz_log(buffer);

    pthread_mutex_lock(&reap_mutex);
    while (atomic_load(&active_children) > 0)
    {
        pthread_cond_wait(&reap_cv, &reap_mutex);
    }
    pthread_mutex_unlock(&reap_mutex);
    running = 0;
    return 0;
}