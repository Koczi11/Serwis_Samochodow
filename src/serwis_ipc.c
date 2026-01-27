#define _GNU_SOURCE

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
#include <signal.h>

//Definicje zmiennych globalnych
int shm_id = -1;    //Pamięć współdzielona
int sem_id = -1;    //Semafory
int msg_id_kierowca = -1;   //Kolejka komunikatów kierowca <-> pracownik
int msg_id_mechanik = -1;   //Kolejka komunikatów mechanik <-> pracownik
int msg_id_kasjer = -1;     //Kolejka komunikatów kasjer <-> pracownik
SharedData *shared = NULL;  //Wskaźnik do pamięci współdzielonej

//Inicjalizacja IPC
//1 - tworzy i czyści zasoby (rodzic)
//0 - dołącza do istniejących zasobów (dziecko)
void init_ipc(int is_parent)
{
    //Generowanie kluczy
    key_t key_shm = ftok(".", 'S');
    key_t key_sem = ftok(".", 'M');
    key_t key_msg_kierowca = ftok(".", 'Q');
    key_t key_msg_mechanik = ftok(".", 'W');
    key_t key_msg_kasjer = ftok(".", 'E');

    if (key_shm == -1 || key_sem == -1 || key_msg_kierowca == -1 || key_msg_mechanik == -1 || key_msg_kasjer == -1)
    {
        perror("ftok failed");
        exit(1);
    }

    //Pamięć współdzielona
    if (is_parent)
    {
        //Tworzenie
        shm_id = shmget(key_shm, sizeof(SharedData), IPC_CREAT | 0600);
        if (shm_id == -1)
        {
            perror("shmget failed (parent)");
            exit(1);
        }
    }
    else
    {
        //Dołączanie
        shm_id = shmget(key_shm, sizeof(SharedData), 0600);
        if (shm_id == -1)
        {
            if (errno == ENOENT || errno == EIDRM)
            {
                exit(0);
            }

            perror("shmget failed (child)");
            exit(1);
        }
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
        memset(shared, 0, sizeof(SharedData));

        shared->serwis_otwarty = 0;
        shared->pozar = 0;
        shared->reset_po_pozarze = 0;
            shared->liczba_oczekujacych_klientow = 0;
            shared->liczba_czekajacych_na_otwarcie = 0;
        shared->aktywne_okienka_obslugi = 1;
        shared->auta_w_serwisie = 0;
            shared->pid_kierownik = -1;

        for (int i = 0; i < MAX_STANOWISK; i++)
        {
            shared->stanowiska[i].zajete = 0;
            shared->stanowiska[i].pid_mechanika = -1;
            shared->stanowiska[i].przyspieszone = 0;
        }

        //Semafory
        sem_id = semget(key_sem, NUM_SEM, IPC_CREAT | 0600);
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

        if (semctl(sem_id, SEM_STANOWISKA, SETVAL, 1) == -1)
        {
            perror("semctl SEM_STANOWISKA failed");
            exit(1);
        }

        if (semctl(sem_id, SEM_LICZNIKI, SETVAL, 1) == -1)
        {
            perror("semctl SEM_LICZNIKI failed");
            exit(1);
        }

        if (semctl(sem_id, SEM_STATUS, SETVAL, 1) == -1)
        {
            perror("semctl SEM_STATUS failed");
            exit(1);
        }

        if (semctl(sem_id, SEM_TIMER, SETVAL, 0) == -1)
        {
            perror("semctl SEM_TIMER failed");
            exit(1);
        }

        //Kolejki komunikatów
        msg_id_kierowca = msgget(key_msg_kierowca, IPC_CREAT | 0600);
        if (msg_id_kierowca == -1)
        {
            perror("msgget kierowca failed");
            exit(1);
        }

        msg_id_mechanik = msgget(key_msg_mechanik, IPC_CREAT | 0600);
        if (msg_id_mechanik == -1)
        {
            perror("msgget mechanik failed");
            exit(1);
        }

        msg_id_kasjer = msgget(key_msg_kasjer, IPC_CREAT | 0600);
        if (msg_id_kasjer == -1)
        {
            perror("msgget kasjer failed");
            exit(1);
        }
    }
    else
    {
        //Procesy potomne dołączają do istniejących zasobów
        sem_id = semget(key_sem, NUM_SEM, 0600);
        if (sem_id == -1)
        {
            if (errno == ENOENT || errno == EIDRM)
            {
                exit(0);
            }

            perror("semget failed");
            exit(1);
        }
        msg_id_kierowca = msgget(key_msg_kierowca, 0600);
        if (msg_id_kierowca == -1)
        {
            if (errno == ENOENT || errno == EIDRM)
            {
                exit(0);
            }

            perror("msgget kierowca failed");
            exit(1);
        }

        msg_id_mechanik = msgget(key_msg_mechanik, 0600);
        if (msg_id_mechanik == -1)
        {
            if (errno == ENOENT || errno == EIDRM)
            {
                exit(0);
            }

            perror("msgget mechanik failed");
            exit(1);
        }

        msg_id_kasjer = msgget(key_msg_kasjer, 0600);
        if (msg_id_kasjer == -1)
        {
            if (errno == ENOENT || errno == EIDRM)
            {
                exit(0);
            }
            
            perror("msgget kasjer failed");
            exit(1);
        }
    }

}

//Czyszczenie zasobów IPC
void cleanup_ipc()
{
    //Odłączanie pamięci współdzielonej
    if (shared != NULL)
    {
        if (shmdt(shared) == -1)
        {
            perror("shmdt failed");
        }
    }

    //Usuwanie segmentu pamięci współdzielonej
    if (shm_id != -1)
    {
        if (shmctl(shm_id, IPC_RMID, NULL) == -1)
        {
            perror("shmctl IPC_RMID failed");
        }
    }

    //Usuwanie zbioru semaforów
    if (sem_id != -1)
    {
        if (semctl(sem_id, 0, IPC_RMID) == -1)
        {
            perror("semctl IPC_RMID failed");
        }
    }

    //Usuwanie kolejek komunikatów
    if (msg_id_kierowca != -1)
    {
        if (msgctl(msg_id_kierowca, IPC_RMID, NULL) == -1)
        {
            perror("msgctl IPC_RMID kierowca failed");
        }
    }
    if (msg_id_mechanik != -1)
    {
        if (msgctl(msg_id_mechanik, IPC_RMID, NULL) == -1)
        {
            perror("msgctl IPC_RMID mechanik failed");
        }
    }
    if (msg_id_kasjer != -1)
    {
        if (msgctl(msg_id_kasjer, IPC_RMID, NULL) == -1)
        {
            perror("msgctl IPC_RMID kasjer failed");
        }
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
        {
            return 1;
        }
    }

    return 0;
}

//Opuszcza semafor
void sem_lock(int num)
{
    struct sembuf sb = {num, -1, SEM_UNDO};
    while (semop(sem_id, &sb, 1) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }

        if (errno == EIDRM || errno == EINVAL)
        {
            exit(0);
        }

        perror("semop lock failed");
        exit(1);
    }
}

//Podnosi semafor
void sem_unlock(int num)
{
    struct sembuf sb = {num, 1, SEM_UNDO};
    while (semop(sem_id, &sb, 1) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }

        if (errno == EIDRM || errno == EINVAL)
        {
            exit(0);
        }

        perror("semop unlock failed");
        exit(1);
    }
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

    if (write(fd, buf, len) == -1)
    {
        perror("write raport.txt failed");
    }
    if (close(fd) == -1)
    {
        perror("close raport.txt failed");
    }
}

void zapisz_log(const char *tekst)
{
    int fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
    {
        perror("open log.txt failed");
        return;
    }

    time_t t = time(NULL);
    char buf[256];

    int len = snprintf(buf, sizeof(buf), "[%ld] %s\n", t, tekst);

    if (write(fd, buf, len) == -1)
    {
        perror("write log.txt failed");
    }
    if (close(fd) == -1)
    {
        perror("close log.txt failed");
    }
}

//Stała tablica usług (Cennik)
static const Usluga CENNIK[MAX_USLUG] = 
{
    {0, "Awaria hamulców (KRYTYCZNA)", 800, 5, 1},
    {1, "Wyciek paliwa (KRYTYCZNA)", 600, 4, 1},
    {2, "Zerwany pasek rozrządu (KRYTYCZNA)", 900, 7, 1},
    {3, "Wymiana oleju", 200, 1, 0},
    {4, "Wymiana opon", 150, 2, 0},
    {5, "Wymiana klocków hamulcowych", 300, 2, 0},
    {6, "Wymiana tarcz hamulcowych", 500, 3, 0},
    {7, "Wymiana płynu chłodniczego", 150, 1, 0},
    {8, "Wymiana płynu hamulcowego", 120, 1, 0},
    {9, "Serwis klimatyzacji", 250, 2, 0},
    {10, "Wymiana filtra powietrza", 80, 1, 0},
    {11, "Wymiana filtra kabinowego", 90, 1, 0},
    {12, "Wymiana świec zapłonowych", 180, 1, 0},
    {13, "Wymiana akumulatora", 350, 1, 0},
    {14, "Wymiana żarówek", 50, 1, 0},
    {15, "Wymiana wycieraczek", 80, 1, 0},
    {16, "Diagnostyka komputerowa", 150, 1, 0},
    {17, "Ustawienie zbieżności", 200, 2, 0},
    {18, "Naprawa zawieszenia (przód)", 800, 4, 0},
    {19, "Naprawa zawieszenia (tył)", 700, 4, 0},
    {20, "Wymiana sprzęgła", 1200, 5, 0},
    {21, "Regeneracja turbosprężarki", 1800, 6, 0},
    {22, "Wymiana alternatora", 600, 2, 0},
    {23, "Wymiana rozrusznika", 550, 2, 0},
    {24, "Wymiana tłumika", 400, 1, 0},
    {25, "Pranie tapicerki", 300, 3, 0},
    {26, "Polerowanie lakieru", 500, 3, 0},
    {27, "Wymiana termostatu", 200, 1, 0},
    {28, "Wymiana uszczelki pod głowicą", 2500, 4, 0},
    {29, "Prostowanie felg", 150, 1, 0}
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

//Wysyła komunikat do kolejki
int send_msg(int msg_id, Msg *msg)
{
    if (msgsnd(msg_id, msg, sizeof(Samochod), 0) == -1)
    {
        if (errno == EIDRM || errno == EINVAL)
        {
            return -2;
        }

        if (errno == EINTR)
        {
            return -1;
        }
        
        perror("msgsnd failed");
        return -1;
    }

    return 0;
}

//Odbiera komunikat z kolejki
int recv_msg(int msg_id, Msg *msg, long type, int flags)
{
    ssize_t wynik = msgrcv(msg_id, msg, sizeof(Samochod), type, flags);

    if (wynik == -1)
    {
        if (errno == EIDRM || errno == EINVAL)
        {
            exit(0);
        }

        if (errno == EINTR)
        {
            return -1;
        }

        if (errno == ENOMSG)
        {
            return -1;
        }

        perror("msgrcv failed");
        return -1;
    }

    return 0;
}

//Czyści kolejkę komunikatów (usuwa wszystkie zaległe wiadomości)
void drain_msg_queue()
{
    Msg msg;

    int msg_ids[] = {msg_id_kierowca, msg_id_mechanik, msg_id_kasjer};
    for (size_t i = 0; i < 3; i++)
    {
        int id = msg_ids[i];
        if (id == -1)
        {
            continue;
        }

        while (msgrcv(id, &msg, sizeof(Samochod), 0, IPC_NOWAIT) != -1)
        {
            //Usunięto wiadomość
        }
    }
}

//Czyści semafor SEM_TIMER
void clear_wakeup_sems()
{
    struct sembuf sb = {SEM_TIMER, -1, IPC_NOWAIT};
    while (semop(sem_id, &sb, 1) != -1)
    {
        //Czyszczenie semafora
    }
}

//Funkcja bezpiecznego oczekiwania
int safe_wait_seconds(double seconds)
{
    if (seconds <= 0)
    {
        return 0;
    }

    struct sembuf sb;
    sb.sem_num = SEM_TIMER;
    sb.sem_op = -1;
    sb.sem_flg = 0;

    struct timespec timeout;
    timeout.tv_sec = (time_t)seconds;
    timeout.tv_nsec = (long)((seconds - timeout.tv_sec) * 1e9);

    if (semtimedop(sem_id, &sb, 1, &timeout) == -1)
    {
        if (errno == EAGAIN)
        {
            return 0;
        }

        if (errno == EINTR)
        {
            return -1;
        }

        if (errno == EIDRM || errno == EINVAL)
        {
            exit(0);
        }

        perror("semtimedop failed");
        return -1;
    }

    return 0;
}

//Dołącza proces do grupy procesu kierownika (dla globalnych sygnałów)
void join_service_group()
{
    if (shared == NULL)
    {
        return;
    }

    pid_t pgid = shared->pid_kierownik;
    if (pgid <= 0)
    {
        return;
    }

    if (setpgid(0, pgid) == -1)
    {
        if (errno != EACCES && errno != EPERM)
        {
            perror("setpgid join_service_group failed");
        }
    }
}