#define _DEFAULT_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

int main()
{
    //Dołączenie do IPC
    init_ipc(0);

    srand(time(NULL));

    printf("[KIEROWNIK] Uruchomiony. Zaczynamy dzień!\n");

    sem_lock(SEM_SHARED);
    shared->aktualna_godzina = 6;
    shared->pozar = 0;
    shared->serwis_otwarty = 0;
    sem_unlock(SEM_SHARED);

    while (1)
    {
        //Symulacja upływu czasu (1 godzina = 5 sekund)
        sleep(5);

        sem_lock(SEM_SHARED);

        //Inkrementacja godziny i pętla doby
        shared->aktualna_godzina++;
        if (shared->aktualna_godzina > 23)
        {
            shared->aktualna_godzina = 0;
        }

        int godzina = shared->aktualna_godzina;
        int pozar = shared->pozar;
        int otwarte = shared->serwis_otwarty;

        //Reset pożaru o 5:00
        if (godzina == 5)
        {
            shared->pozar = 0;
            pozar = 0;
        }

        //Decyzja o otwarciu/zamknięciu serwisu
        if (godzina >= GODZINA_OTWARCIA && godzina < GODZINA_ZAMKNIECIA)
        {
            if (!otwarte && !pozar)
            {
                printf("[KIEROWNIK] Godzina %d:00. Otwieram serwis\n", godzina);
                shared->serwis_otwarty = 1;
                otwarte = 1;

                sem_unlock(SEM_SHARED);
                signal_serwis_otwarty();
                sem_lock(SEM_SHARED);
            }
        }
        else
        {
            if (otwarte)
            {
                printf("[KIEROWNIK] Godzina %d:00. Zamykam serwis\n", godzina);
                shared->serwis_otwarty = 0;
                otwarte = 0;
            }
        }
        sem_unlock(SEM_SHARED);

        //Informacje o stanie serwisu
        if (!otwarte)
        {
            if (pozar)
            {
                printf("[KIEROWNIK] Godzina %d:00. Trwa sprzątanie po pożarze\n", godzina); 
            }
            else
            {
                printf("[KIEROWNIK] Godzina %d:00. Serwis jest zamknięty\n", godzina); 
            }
            continue;
        }

        //Losowe zdarzenia
        //Kierownik losowo wpływa na pracę mechaników

        int stanowisko = rand() % MAX_STANOWISK;

        //Pobieramy PID mechanika z wylosowanego stanowiska
        sem_lock(SEM_SHARED);
        pid_t pid = shared->stanowiska[stanowisko].pid_mechanika;
        sem_unlock(SEM_SHARED);

        //Stanowisko jest puste
        if (pid <= 0)
            continue;

        //10% szansy na zdarzenie
        int los = rand () % 100;
        if (los < 10)
        {
            int akcja = rand() % 4;

            switch (akcja)
            {
                //Sygnał 1: Zamknięcie stanowiska
                case 0:
                    printf("[KIEROWNIK] Zamknięcie stanowiska %d\n", stanowisko);
                    if (kill(pid, SIGUSR1) == -1) 
                    {
                        perror("[KIEROWNIK] Błąd wysłania SIGUSR1");
                    }
                    break;

                //Sygnał 2: Przyspieszenie pracy stanowiska
                case 1:
                    printf("[KIEROWNIK] Przyspieszenie stanowiska %d\n", stanowisko);
                    if (kill(pid, SIGUSR2) == -1) 
                    {
                        perror("[KIEROWNIK] Błąd wysłania SIGUSR2");
                    }
                    break;

                //Sygnał 3: Wznowienie normalnej pracy stanowiska
                case 2:
                    printf("[KIEROWNIK] Normalna praca stanowiska %d\n", stanowisko);
                    if (kill(pid, SIGRTMIN) == -1) 
                    {
                        perror("[KIEROWNIK] Błąd wysłania SIGRTMIN");
                    }
                    break;
                    
                //Sygnał 4: Pożar w serwisie
                case 3:
                    printf("[KIEROWNIK] POŻAR!\n");
                    sem_lock(SEM_SHARED);
                    shared->serwis_otwarty = 0;
                    shared->pozar = 1;
                    shared->auta_w_serwisie = 0;
                    shared->liczba_oczekujacych_klientow = 0;

                    for (int i = 0; i < MAX_STANOWISK; i++)
                    {
                        if (shared->stanowiska[i].pid_mechanika > 0)
                        {
                            if (kill(shared->stanowiska[i].pid_mechanika, SIGTERM) == -1)
                            {
                                perror("[KIEROWNIK] Błąd wysłania SIGTERM");
                            }
                        }
                    }

                    sem_unlock(SEM_SHARED);
                    break;
            }
        }
    }
    return 0;
}