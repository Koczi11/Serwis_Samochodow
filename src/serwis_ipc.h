#ifndef SERWIS_IPC_H
#define SERWIS_IPC_H

#include <sys/types.h>

//STAŁE
#define MAX_STANOWISK 8
#define MAX_USLUG 30
#define MAX_MARKA_LEN 2

#define GODZINA_OTWARCIA 8      
#define GODZINA_ZAMKNIECIA 18 
#define LIMIT_OCZEKIWANIA 1

//TYPY KOMUNIKATÓW
#define MSG_REJESTRACJA 1
#define MSG_DECYZJA_USLUGI 3
#define MSG_DECYZJA_DODATKOWA 23
#define MSG_KASA 30
#define MSG_OD_MECHANIKA 40

//SEMAFORY
#define SEM_SHARED 0                //Główny semafor do ochrony pamięci współdzielonej

//STRUKTURY DANYCH

//Opis usługi
typedef struct
{
    int id_uslugi;
    char nazwa[100];
    int koszt;
    int czas_wykonania;
    int krytyczna;
} Usluga;


//Opis samochodu
typedef struct 
{
    pid_t pid_kierowcy;
    char marka[MAX_MARKA_LEN];

    int id_uslugi;
    int czas_naprawy;
    int koszt;
    int zaakceptowano;
    
    int id_stanowiska_roboczego;

    int dodatkowa_usterka;
    int id_dodatkowej_uslugi;
    int dodatkowy_koszt;
    int dodatkowy_czas;

    int ewakuacja;
}Samochod;

//Stan stanowiska
typedef struct 
{
    int zajete;
    pid_t pid_mechanika;
    int przyspieszone;
} Stanowisko;

//Główna struktura pamięci współdzielonej
typedef struct
{
    Stanowisko stanowiska[MAX_STANOWISK];
    int serwis_otwarty;
    int aktualna_godzina;
    int pozar;
    int liczba_oczekujacych_klientow;
    int aktywne_okienka_obslugi;
    int auta_w_serwisie;
} SharedData;

//Komunikat w kolejce 
typedef struct 
{
    long mtype;
    Samochod samochod;
} Msg;

//Zmienne globalne
extern int shm_id;
extern int sem_id;
extern int msg_id;
extern SharedData *shared;

//FUNKCJE

void init_ipc(int is_parent);
void cleanup_ipc();

int marka_obslugiwana(const char *m);

//Operacje semaforowe
void sem_lock(int num);
void sem_unlock(int num);

void zapisz_raport(const char *tekst);
Usluga pobierz_usluge(int id);

int send_msg(int msg_id, Msg *msg);

int recv_msg(int msg_id, Msg *msg, long type, int flags);

#endif