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
#define MSG_KASA 30
#define MSG_OD_MECHANIKA 40
#define MSG_WOLNY_MECHANIK 41

#define MSG_CTRL_OPEN_KIEROWCA 60000
#define MSG_CTRL_OPEN_PRACOWNIK 15000
#define MSG_CTRL_OPEN_MECHANIK 80
#define MSG_CTRL_OPEN_KASJER 70

//Typy dla komunikatów kierowców (unikamy kolizji z innymi mtype)
#define MSG_KIEROWCA_BASE 100000
#define MSG_KIEROWCA(pid) (MSG_KIEROWCA_BASE + (pid))

#define MSG_PRACOWNIK_BASE(id) (10000 + (id) * 1000)
#define MSG_DECYZJA_USLUGI(id) (MSG_PRACOWNIK_BASE(id) + 1)
#define MSG_DECYZJA_DODATKOWA(id) (MSG_PRACOWNIK_BASE(id) + 2)
#define MSG_POTWIERDZENIE_PLATNOSCI(id) (MSG_PRACOWNIK_BASE(id) + 3)

//Typy per-kierowca (dla obsługi wielowątkowej pracowników)
#define MSG_DECYZJA_USLUGI_BASE 200000
#define MSG_DECYZJA_USLUGI_PID(pid) (MSG_DECYZJA_USLUGI_BASE + (pid))
#define MSG_DECYZJA_DODATKOWA_BASE 300000
#define MSG_DECYZJA_DODATKOWA_PID(pid) (MSG_DECYZJA_DODATKOWA_BASE + (pid))
#define MSG_POTWIERDZENIE_PLATNOSCI_BASE 400000
#define MSG_POTWIERDZENIE_PLATNOSCI_PID(pid) (MSG_POTWIERDZENIE_PLATNOSCI_BASE + (pid))
#define MSG_MECHANIK_EVENT_BASE 500000
#define MSG_MECHANIK_EVENT_PID(pid) (MSG_MECHANIK_EVENT_BASE + (pid))

//SEMAFORY
#define SEM_SHARED 0                //Globalny semafor awaryjny
#define SEM_STANOWISKA 1            //Ochrona shared->stanowiska[]
#define SEM_LICZNIKI 2              //Ochrona liczników w shared
#define SEM_STATUS 3                //Ochrona statusu serwisu (godzina/pożar/otwarcie)
#define SEM_TIMER 4                 //Semafor do implementacji bezpiecznego oczekiwania z timeoutem

#define NUM_SEM 5                   //Liczba semaforów

//STAŁE KONFIGURACYJNE
#define LICZBA_PRACOWNIKOW 3


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
    int id_pracownika;
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
    int reset_po_pozarze;
    int liczba_oczekujacych_klientow;
    int liczba_czekajacych_na_otwarcie;
    int aktywne_okienka_obslugi;
    int auta_w_serwisie;
    pid_t pid_kierownik;
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
extern int msg_id_kierowca;
extern int msg_id_mechanik;
extern int msg_id_kasjer;
extern SharedData *shared;

//FUNKCJE

void init_ipc(int is_parent);
void cleanup_ipc();

int marka_obslugiwana(const char *m);

//Operacje semaforowe
void sem_lock(int num);
void sem_unlock(int num);

void zapisz_raport(const char *tekst);
void zapisz_log(const char *tekst);
Usluga pobierz_usluge(int id);

int send_msg(int msg_id, Msg *msg);

int recv_msg(int msg_id, Msg *msg, long type, int flags);

void drain_msg_queue();
void clear_wakeup_sems();

int safe_wait_seconds(double seconds);

void join_service_group();

#endif