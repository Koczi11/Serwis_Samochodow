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

//Opis samochodu 
typedef struct 
{
    pid_t pid_kierowcy;
    char marka[MAX_MARKA_LEN];
    int usterka_krytyczna;
    int czas_naprawy;
    int koszt;
    int zaakceptowano;
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



extern int shm_id;
extern int sem_id;
extern int msg_id;
extern SharedData *shared;

void init_ipc();
void cleanup_ipc();

#endif
