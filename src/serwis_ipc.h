#ifndef SERWIS_IPC_H
#define SERWIS_IPC_H

#include <sys/types.h>

#define MAX_STANOWISK 8
#define MAX_USLUG 30
#define MAX_MARKA_LEN 2

//Typy procesów
#define PROC_KIERWOCA 1
#define PROC_PRACOWNIK_SERWISU 2
#define PROC_MECHANIK 3
#define PROC_KASJER 4
#define PROC_KIEROWNIK 5

//Typy komunikatów
#define MSG_REJESTRACJA 1
#define MSG_WYCENA 2
#define MSG_DECYZJA 3

#define MSG_NAPRAWA 10
#define MSG_KONIEC_NAPRAWY 11

#define MSG_USTERKA 20
#define MSG_PYTANIE 21
#define MSG_ODPOWIEDZ 22
#define MSG_DECYZJA 23

#define MSG_KASA 30
#define MSG_ZAPLATA 31

//Semafory
#define SEM_SHARED 0        //Ochrona pamięci współdzielonej
#define SEM_STANOWISKA 1    //Synchronizacja stanowisk

//Opis samochodu
typedef struct 
{
    pid_t pid_kierowcy;
    char marka[MAX_MARKA_LEN];
    int usterka_krytyczna;
    int czas_naprawy;
    int koszt;
    int zaakceptowano;
    int dodatkowa_usterka;
    int dodatkowy_koszt;
    int dodatkowy_czas;
}Samochod;

//Stan stanowiska
typedef struct 
{
    int zajete;
    pid_t pid_mechanika;
    int przyspieszone;
} Stanowisko;

//Pamięć współdzielona serwisu
typedef struct
{
    Stanowisko stanowiska[MAX_STANOWISK];
    int serwis_otwarty;
    int pozar;
} SharedData;

//Komunikat w kolejce 
typedef struct 
{
    long mtype;
    Samochod samochod;
} Msg;

//Identyfikatory IPC
extern int shm_id;
extern int sem_id;
extern int msg_id;
extern SharedData *shared;

//Funkcje
void init_ipc();
void cleanup_ipc();

int marka_obslugiwana(const char *m);

void sem_lock(int num);
void sem_unlock(int num);

#endif