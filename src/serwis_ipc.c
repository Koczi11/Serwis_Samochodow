#include "serwis_ipc.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

int shm_id = -1;
int sem_id = -1;
int msg_id = -1;
SharedData *shared = NULL;

//Inicjalizacja IPC
void init_ipc(int is_parent)
{
    key_t key_shm = ftok(".", 'S');
    key_t key_sem = ftok(".", 'M');
    key_t key_msg = ftok(".", 'Q');

    if (key_shm == -1 || key_sem == -1 || key_msg == -1)
    {
        perror("ftok failed");
        exit(1);
    }

    //Pamięć współdzielona
    if (is_parent)
    {
        shm_id = shmget(key_shm, sizeof(SharedData), IPC_CREAT | 0600);
    }
    else
    {
        shm_id = shmget(key_shm, sizeof(SharedData), 0600);
    }

    if (shm_id == -1)
    {
        perror("shmget failed");
        exit(1);
    }

    shared = (SharedData *)shmat(shm_id, NULL, 0);

    if (shared == (void *)-1)
    {
        perror("shmat failed");
        exit(1);
    }

    //Inicjalizacja danych
    if (is_parent)
    {
        shared->serwis_otwarty = 0;
        shared->pozar = 0;
        shared->liczba_oczekujacych_klientow = 0;
        shared->aktywne_okienka_obslugi = 1;

        for (int i = 0; i < MAX_STANOWISK; i++)
        {
            shared->stanowiska[i].zajete = 0;
            shared->stanowiska[i].pid_mechanika = -1;
            shared->stanowiska[i].przyspieszone = 0;
        }

        //Semafor
        sem_id = semget(key_sem, 2, IPC_CREAT | 0600);
        if (sem_id == -1)
        {
            perror("semget failed");
            exit(1);
        }

        if (semctl(sem_id, SEM_SHARED, SETVAL, 1) == -1)
        {
            perror("semctl SEM_SHARED failed");
            exit(1);
        }

        if (semctl(sem_id, SEM_STANOWISKA, SETVAL, 1) == -1)
        {
            perror("semctl SEM_STANOWISKA failed");
            exit(1);
        }
    
        //Kolejka komunikatów
        msg_id = msgget(key_msg, IPC_CREAT | 0600);
        if (msg_id == -1)
        {
            perror("msgget failed");
            exit(1);
        }
    }
    else
    {
        sem_id = semget(key_sem, 2, 0600);
        if (sem_id == -1)
        {
            perror("semget failed");
            exit(1);
        }

        msg_id = msgget(key_msg, 0600);
        if (msg_id == -1)
        {
            perror("msgget failed");
            exit(1);
        }
    }

    printf(is_parent? "[INIT] Utworzono IPC\n" : "[INIT] Dolaczono do IPC\n");
}

//Czyszczenie IPC
void cleanup_ipc()
{
    if (shared != NULL)
    {
        shmdt(shared);
    }

    if (shm_id != -1)
    {
        shmctl(shm_id, IPC_RMID, NULL);
    }

    if (sem_id != -1)
    {
        semctl(sem_id, 0, IPC_RMID);
    }

    if (msg_id != -1)
    {
        msgctl(msg_id, IPC_RMID, NULL);
    }

    printf("[CLEANUP] IPC zwolnione poprawnie\n");
}

//Sprawdza czy marka jest obsługiwana
int marka_obslugiwana(const char *m)
{
    const char *dozwolone[] = {"A", "E", "I", "O", "U", "Y",};
    for (int i = 0; i < 6; i++)
    {
        if (strcmp(m, dozwolone[i]) == 0)
            return 1;
    }

    return 0;
}

//Operacje semaforowe
void sem_lock(int num)
{
    struct sembuf sb = {num, -1, 0};
    semop(sem_id, &sb, 1);
}

void sem_unlock(int num)
{
    struct sembuf sb = {num, 1, 0};
    semop(sem_id, &sb, 1);
}

void zapisz_raport(const char *tekst)
{
    int fd = open("raport.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
    {
        perror("open raport.txt failed");
        return;
    }

    time_t t = time(NULL);
    char buf[256];

    int len = snprintf(buf, sizeof(buf), "[%ld] %s\n", t, tekst);

    write(fd, buf, len);
    close(fd);
}