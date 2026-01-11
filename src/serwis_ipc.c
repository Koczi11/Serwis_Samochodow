#include "serwis_ipc.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

//Definicje zmiennych globalnych
int shm_id = -1;    //Pamięć współdzielona
int sem_id = -1;    //Semafory
int msg_id = -1;    //Kolejka komunikatów
SharedData *shared = NULL;  //Wskaźnik do pamięci współdzielonej

//Inicjalizacja IPC
//1 - tworzy i czyści zasoby (rodzic)
//0 - dołącza do istniejących zasobów (dziecko)
void init_ipc(int is_parent)
{
    //Generowanie kluczy
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
        //Tworzenie
        shm_id = shmget(key_shm, sizeof(SharedData), IPC_CREAT | 0600);
    }
    else
    {
        //Dołączanie
        shm_id = shmget(key_shm, sizeof(SharedData), 0600);
    }

    if (shm_id == -1)
    {
        perror("shmget failed");
        exit(1);
    }

    //Mapowanie pamięci do przestrzeni adresowej procesu
    shared = (SharedData *)shmat(shm_id, NULL, 0);

    if (shared == (void *)-1)
    {
        perror("shmat failed");
        exit(1);
    }

    //Inicjalizacja struktur danych
    if (is_parent)
    {
        shared->serwis_otwarty = 0;
        shared->pozar = 0;
        shared->liczba_oczekujacych_klientow = 0;
        shared->aktywne_okienka_obslugi = 1;
        shared->auta_w_serwisie = 0;

        for (int i = 0; i < MAX_STANOWISK; i++)
        {
            shared->stanowiska[i].zajete = 0;
            shared->stanowiska[i].pid_mechanika = -1;
            shared->stanowiska[i].przyspieszone = 0;
        }

        //Semafory
        sem_id = semget(key_sem, 1, IPC_CREAT | 0600);
        if (sem_id == -1)
        {
            perror("semget failed");
            exit(1);
        }

        //Ustawianie wartości początkowej semaforów
        if (semctl(sem_id, SEM_SHARED, SETVAL, 1) == -1)
        {
            perror("semctl SEM_SHARED failed");
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
        //Procesy potomne dołączają do istniejących zasobów
        sem_id = semget(key_sem, 1, 0600);
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

    //Log dla debugowania
    //printf(is_parent? "[INIT] Utworzono IPC\n" : "[INIT] Dolaczono do IPC\n");
}

//Czyszczenie zasobów IPC
void cleanup_ipc()
{
    //Odłączanie pamięci współdzielonej
    if (shared != NULL)
    {
        shmdt(shared);
    }

    //Usuwanie segmentu pamięci współdzielonej
    if (shm_id != -1)
    {
        shmctl(shm_id, IPC_RMID, NULL);
    }

    //Usuwanie zbioru semaforów
    if (sem_id != -1)
    {
        semctl(sem_id, 0, IPC_RMID);
    }

    //Usuwanie kolejki komunikatów
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

//Opuszcza semafor
void sem_lock(int num)
{
    struct sembuf sb = {num, -1, 0};
    semop(sem_id, &sb, 1);
}

//Podnosi semafor
void sem_unlock(int num)
{
    struct sembuf sb = {num, 1, 0};
    semop(sem_id, &sb, 1);
}

//Zapisuje log do pliku tekstowego raport.txt
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

//Stała tablica usług (Cennik)
static const Usluga CENNIK[MAX_USLUG] = 
{
    {0, "Awaria hamulców (KRYTYCZNA)", 800, 10, 1},
    {1, "Wyciek paliwa (KRYTYCZNA)", 600, 8, 1},
    {2, "Zerwany pasek rozrządu (KRYTYCZNA)", 900, 15, 1},
    {3, "Wymiana oleju", 200, 3, 0},
    {4, "Wymiana opon", 150, 4, 0},
    {5, "Wymiana klocków hamulcowych", 300, 5, 0},
    {6, "Wymiana tarcz hamulcowych", 500, 6, 0},
    {7, "Wymiana płynu chłodniczego", 150, 2, 0},
    {8, "Wymiana płynu hamulcowego", 120, 2, 0},
    {9, "Serwis klimatyzacji", 250, 4, 0},
    {10, "Wymiana filtra powietrza", 80, 1, 0},
    {11, "Wymiana filtra kabinowego", 90, 1, 0},
    {12, "Wymiana świec zapłonowych", 180, 3, 0},
    {13, "Wymiana akumulatora", 350, 1, 0},
    {14, "Wymiana żarówek", 50, 1, 0},
    {15, "Wymiana wycieraczek", 80, 1, 0},
    {16, "Diagnostyka komputerowa", 150, 2, 0},
    {17, "Ustawienie zbieżności", 200, 5, 0},
    {18, "Naprawa zawieszenia (przód)", 800, 8, 0},
    {19, "Naprawa zawieszenia (tył)", 700, 8, 0},
    {20, "Wymiana sprzęgła", 1200, 12, 0},
    {21, "Regeneracja turbosprężarki", 1800, 14, 0},
    {22, "Wymiana alternatora", 600, 5, 0},
    {23, "Wymiana rozrusznika", 550, 4, 0},
    {24, "Wymiana tłumika", 400, 3, 0},
    {25, "Pranie tapicerki", 300, 6, 0},
    {26, "Polerowanie lakieru", 500, 8, 0},
    {27, "Wymiana termostatu", 200, 3, 0},
    {28, "Wymiana uszczelki pod głowicą", 2500, 20, 0},
    {29, "Prostowanie felg", 150, 4, 0}
};

//Pobiera opis usługi na podstawie ID
Usluga pobierz_usluge(int id)
{
    if (id < 0 || id >= MAX_USLUG)
    {
        return CENNIK[3]; //domyślna usługa: Wymiana oleju
    }
    
    return CENNIK[id];
}