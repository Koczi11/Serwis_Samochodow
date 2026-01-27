#define _DEFAULT_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/msg.h>
#include <errno.h>
#include <sys/wait.h>

//Progi do otwierania dodatkowych okienek
#define K1 3
#define K2 5

//Funkcja sprawdzająca długość kolejki oczekujących klientów
int sprawdz_dlugosc_kolejki()
{
    int liczba = 0;
    sem_lock(SEM_LICZNIKI);
    liczba = shared->liczba_oczekujacych_klientow;
    sem_unlock(SEM_LICZNIKI);
    return liczba;
}

//Funkcja znajdująca wolnego mechanika mogącego obsłużyć auto
int znajdz_wolne_stanowisko(const char *marka)
{
    int is_UY = (strcmp(marka, "U") == 0 || strcmp(marka, "Y") == 0);

    sem_lock(SEM_STANOWISKA);
    for (int i = 0; i < MAX_STANOWISK; i++)
    {
        if (i == 7 && !is_UY)
            continue;

        int zajete = shared->stanowiska[i].zajete;
        pid_t pid = shared->stanowiska[i].pid_mechanika;

        //Sprawdzamy czy stanowisko jest wolne
        if (!zajete && pid > 0)
        {
            //Oznaczamy stanowisko jako zajęte
            shared->stanowiska[i].zajete = 1;
            sem_unlock(SEM_STANOWISKA);
            return i;
        }
    }
    sem_unlock(SEM_STANOWISKA);
    return -1;
}

//Funkcja sprawdzająca czy są aktywni mechanicy
int aktywni_mechanicy()
{
    int aktywni = 0;
    sem_lock(SEM_STANOWISKA);
    for (int i = 0; i < MAX_STANOWISK; i++)
    {
        if (shared->stanowiska[i].zajete)
        {
            aktywni = 1;
            break;
        }
    }
    sem_unlock(SEM_STANOWISKA);
    return aktywni;
}

//Flaga ewakuacji
volatile sig_atomic_t ewakuacja = 0;

//Obsługa sygnału pożaru
void handle_pozar(int sig)
{
    (void)sig;
    ewakuacja = 1;
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        char arg_buff[16];

        for (int i = 0; i < LICZBA_PRACOWNIKOW; i++)
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                snprintf(arg_buff, sizeof(arg_buff), "%d", i);
                execl("./pracownik_serwisu", "pracownik_serwisu", arg_buff, NULL);
                perror("execl pracownik_serwisu failed");
                exit(1);
            }
            else if (pid < 0)
            {
                perror("fork pracownik_serwisu failed");
                exit(1);
            }
        }

        while (wait(NULL) > 0);
        return 0;
    }

    if (argc < 2)
    {
        fprintf(stderr, "Brak ID pracownika serwisu\n");
        exit(1);
    }

    int id_pracownika = atoi(argv[1]);

    //Dołączenie do IPC
    init_ipc(0);
    join_service_group();

    //Rejestracja handlera pożaru
    struct sigaction sa;
    sa.sa_handler = handle_pozar;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1)
    {
        perror("sigaction SIGUSR1 failed");
    }

    //Pierwszy pracownik serwisu jest zawsze aktywny
    int czy_aktywny = (id_pracownika == 0) ? 1 : 0;

    Msg msg;
    char buffer[256];

    printf("[PRACOWNIK SERWISU %d] Uruchomiony\n", id_pracownika);
    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Uruchomiony", id_pracownika);
    zapisz_log(buffer);

    //Pętla dni pracy
    while (1)
    {
        if (ewakuacja)
        {
            ewakuacja = 0;
        }

        //Czekanie na otwarcie serwisu (komunikat)
        while (1)
        {
            int r = recv_msg(msg_id_kierowca, &msg, MSG_CTRL_OPEN_PRACOWNIK, 0);
            if (r == 0)
            {
                break;
            }
            if (r == -2)
            {
                exit(0);
            }

            if (errno == EINTR && ewakuacja)
            {
                printf("[PRACOWNIK SERWISU %d] Otrzymano sygnał pożaru! Uciekam!\n", id_pracownika);
                snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Otrzymano sygnał pożaru! Uciekam!", id_pracownika);
                zapisz_log(buffer);
                ewakuacja = 0;
                continue;
            }
        }

        if (ewakuacja)
        {
            printf("[PRACOWNIK SERWISU %d] Otrzymano sygnał pożaru! Uciekam!\n", id_pracownika);
            snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Otrzymano sygnał pożaru! Uciekam!", id_pracownika);
            zapisz_log(buffer);
            ewakuacja = 0;
            continue;
        }

        //Reset flagi aktywności na początek dnia
        if (id_pracownika != 0)
        {
            czy_aktywny = 0;
        }

        printf("[PRACOWNIK SERWISU %d] Serwis otwarty, zaczynam pracę\n", id_pracownika);
        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Serwis otwarty, zaczynam pracę", id_pracownika);
        zapisz_log(buffer);

        //Pętla obsługi klientów w ciągu dnia
        while (1)
        {
            if (ewakuacja)
            {
                printf("[PRACOWNIK SERWISU %d] Otrzymano sygnał pożaru! Uciekam!\n", id_pracownika);
                snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Otrzymano sygnał pożaru! Uciekam!", id_pracownika);
                zapisz_log(buffer);

                break;
            }

            sem_lock(SEM_STATUS);
            int otwarte = shared->serwis_otwarty;
            sem_unlock(SEM_STATUS);

            //Sterowanie otwieraniem/zamykaniem dodatkowych stanowisk
            if (otwarte)
            {
                int q_len = sprawdz_dlugosc_kolejki();

                //Logika dla Pracownika Serwisu 1 (stanowisko 2)
                if (id_pracownika == 1)
                {
                    if (!czy_aktywny && q_len >= K1)
                    {
                        czy_aktywny = 1;
                        printf("[PRACOWNIK SERWISU %d] Kolejka %d >= %d. Otwieram 2 stanowisko\n", id_pracownika, q_len, K1);
                        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Kolejka %d >= %d. Otwieram 2 stanowisko", id_pracownika, q_len, K1);
                        zapisz_log(buffer);
                    }
                    else if (czy_aktywny && q_len <= 2)
                    {
                        czy_aktywny = 0;
                        printf("[PRACOWNIK SERWISU %d] Kolejka %d <= 2. Zamykam 2 stanowisko\n", id_pracownika, q_len);
                        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Kolejka %d <= 2. Zamykam 2 stanowisko", id_pracownika, q_len);
                        zapisz_log(buffer);
                    }
                }
                //Logika dla Pracownika Serwisu 2 (stanowisko 3)
                else if (id_pracownika == 2)
                {
                    if (!czy_aktywny && q_len >= K2)
                    {
                        czy_aktywny = 1;
                        printf("[PRACOWNIK SERWISU %d] Kolejka %d >= %d. Otwieram 3 stanowisko\n", id_pracownika, q_len, K2);
                        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Kolejka %d >= %d. Otwieram 3 stanowisko", id_pracownika, q_len, K2);
                        zapisz_log(buffer);
                    }
                    else if (czy_aktywny && q_len <= 3)
                    {
                        czy_aktywny = 0;
                        printf("[PRACOWNIK SERWISU %d] Kolejka %d <= 3. Zamykam 3 stanowisko\n", id_pracownika, q_len);
                        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Kolejka %d <= 3. Zamykam 3 stanowisko", id_pracownika, q_len);
                        zapisz_log(buffer);
                    }
                }
            }

            //Jeśli pracownik nie jest aktywny i serwis jest otwarty, pomija rejestrację
            if (!czy_aktywny && otwarte)
            {
                //Obsługa pozostałych komunikatów bez aktywnej rejestracji
            }

            int odebrano = 0;

            //Odbiór komunikatów od mechaników
            if (recv_msg(msg_id_mechanik, &msg, MSG_OD_MECHANIKA, IPC_NOWAIT) != -1)
            {
                odebrano = 1;

                if (msg.samochod.dodatkowa_usterka)
                {
                    //Mechanik zgłosił dodatkową usterkę
                    printf("[PRACOWNIK SERWISU %d] Mechanik ze stanowiska %d zgłosił dodatkową usterkę w aucie %d: %s\n", id_pracownika, msg.samochod.id_stanowiska_roboczego, msg.samochod.pid_kierowcy, pobierz_usluge(msg.samochod.id_dodatkowej_uslugi).nazwa);
                    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Mechanik ze stanowiska %d zgłosił dodatkową usterkę w aucie %d: %s", id_pracownika, msg.samochod.id_stanowiska_roboczego, msg.samochod.pid_kierowcy, pobierz_usluge(msg.samochod.id_dodatkowej_uslugi).nazwa);
                    zapisz_log(buffer);

                    msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                    send_msg(msg_id_kierowca, &msg);
                }
                else
                {
                    //Mechanik zakończył naprawę
                    printf("[PRACOWNIK SERWISU %d] Auto %d ukończone przez mechanika na stanowisku %d\n", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.id_stanowiska_roboczego);
                    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Auto %d ukończone przez mechanika na stanowisku %d", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.id_stanowiska_roboczego);
                    zapisz_log(buffer);

                    //Pracownik serwisu ustala ostateczny koszt na podstawie zakresu wykonanych prac
                    int koszt_podstawowy = pobierz_usluge(msg.samochod.id_uslugi).koszt;
                    int koszt_dodatkowy = (msg.samochod.dodatkowy_koszt > 0) ? msg.samochod.dodatkowy_koszt : 0;
                    msg.samochod.koszt = koszt_podstawowy + koszt_dodatkowy;

                    msg.mtype = MSG_KASA;
                    send_msg(msg_id_kasjer, &msg);
                }

                continue;
            }

            //Odbiór potwierdzenia płatności z kasy i przekazanie do kierowcy
            if (recv_msg(msg_id_kasjer, &msg, MSG_POTWIERDZENIE_PLATNOSCI(id_pracownika), IPC_NOWAIT) != -1)
            {
                odebrano = 1;
                msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                if (send_msg(msg_id_kierowca, &msg) == -1)
                {
                    perror("[PRACOWNIK SERWISU] Błąd wysłania potwierdzenia płatności do kierowcy");
                }
                else
                {
                    
                }
                continue;
            }

            //Odbiór decyzji o dodatkowej usterce od kierowcy
            if (recv_msg(msg_id_kierowca, &msg, MSG_DECYZJA_DODATKOWA(id_pracownika), IPC_NOWAIT) != -1)
            {
                odebrano = 1;
                printf("[PRACOWNIK SERWISU %d] Otrzymano decyzję od kierowcy %d. Przekazuję mechanikowi na stanowisko %d\n", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.id_stanowiska_roboczego);
                snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Otrzymano decyzję od kierowcy %d. Przekazuję mechanikowi na stanowisko %d", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.id_stanowiska_roboczego);
                zapisz_log(buffer);

                //Przekazanie decyzji mechanikowi
                msg.mtype = 100 + msg.samochod.id_stanowiska_roboczego;
                send_msg(msg_id_mechanik, &msg);
                
                continue;
            }


            //Obsługa nowych klientów
            if (czy_aktywny && otwarte)
            {
                //Pobieramy zgłoszenie rejestracji
                if (recv_msg(msg_id_kierowca, &msg, MSG_REJESTRACJA, IPC_NOWAIT) != -1)
                {
                    odebrano = 1;

                    msg.samochod.id_pracownika = id_pracownika;

                    //Aktualizacja liczby oczekujących klientów
                    sem_lock(SEM_LICZNIKI);
                    if (shared->liczba_oczekujacych_klientow > 0)
                    {
                        shared->liczba_oczekujacych_klientow--;
                    }
                    sem_unlock(SEM_LICZNIKI);

                    printf("[PRACOWNIK SERWISU %d] Obsługa kierowcy %d, marka %s, usługa ID: %d\n", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.marka, msg.samochod.id_uslugi);
                    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Obsługa kierowcy %d, marka %s, usługa ID: %d", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.marka, msg.samochod.id_uslugi);
                    zapisz_log(buffer);

                    //Symulacja czasu obsługi przy rejestracji
                    //safe_wait_seconds(1);

                    Usluga u = pobierz_usluge(msg.samochod.id_uslugi);

                    //Wycena naprawy
                    msg.samochod.czas_naprawy = u.czas_wykonania;
                    msg.samochod.koszt = u.koszt;
                    msg.samochod.zaakceptowano = 0;
                    msg.samochod.dodatkowa_usterka = 0;
                    msg.samochod.id_stanowiska_roboczego = -1;

                    printf("[PRACOWNIK SERWISU %d] Wycena dla kierowcy %d: koszt %d, czas naprawy %d\n", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.koszt, msg.samochod.czas_naprawy);
                    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Wycena dla kierowcy %d: koszt %d, czas naprawy %d", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.koszt, msg.samochod.czas_naprawy);
                    zapisz_log(buffer);

                    //Odesłanie wyceny
                    msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                    send_msg(msg_id_kierowca, &msg);

                    //Oczekiwanie na decyzję kierowcy
                    Msg decyzja;
                    int odebrano_decyzje = 0;

                    while (1)
                    {
                        if (ewakuacja)
                        {
                            odebrano_decyzje = -1;
                            break;
                        }

                        sem_lock(SEM_STATUS);
                        int otwarte_po = shared->serwis_otwarty;
                        sem_unlock(SEM_STATUS);
                        if (!otwarte_po)
                        {
                            odebrano_decyzje = -1;
                            break;
                        }

                        int r = recv_msg(msg_id_kierowca, &decyzja, MSG_DECYZJA_USLUGI(id_pracownika), IPC_NOWAIT);
                        if (r == -2)
                        {
                            exit(0);
                        }
                        if (r != -1)
                        {
                            //Sprawdź czy to odpowiedź od właściwego kierowcy
                            if (decyzja.samochod.pid_kierowcy == msg.samochod.pid_kierowcy)
                            {
                                msg = decyzja;
                                odebrano_decyzje = 1;
                                break;
                            }
                            else
                            {
                                //Nieodpowiedni kierowca, odsyłamy z powrotem
                                send_msg(msg_id_kierowca, &decyzja);
                            }
                        }
                        if (errno == EINTR)
                        {
                            continue;
                        }

                        safe_wait_seconds(0.1);
                    }

                    //Wyjście z pętli jeśli był pożar
                    if (odebrano_decyzje == -1)
                    {
                        break;
                    }

                    if (msg.samochod.zaakceptowano)
                    {
                        printf("[PRACOWNIK SERWISU %d] Kierowca %d zaakceptował usługę\n", id_pracownika, msg.samochod.pid_kierowcy);
                        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Kierowca %d zaakceptował usługę", id_pracownika, msg.samochod.pid_kierowcy);
                        zapisz_log(buffer);

                        sem_lock(SEM_STATUS);
                        if (!shared->serwis_otwarty || ewakuacja)
                        {
                            sem_unlock(SEM_STATUS);
                            printf("[PRACOWNIK SERWISU %d] Serwis właśnie zamknięto. Odsyłam kierowcę %d\n", id_pracownika, msg.samochod.pid_kierowcy);
                            snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Serwis właśnie zamknięto. Odsyłam kierowcę %d", id_pracownika, msg.samochod.pid_kierowcy);
                            zapisz_log(buffer);

                            msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                            msg.samochod.koszt = 0;
                            msg.samochod.czas_naprawy = 0;
                            msg.samochod.dodatkowa_usterka = 0;
                            msg.samochod.ewakuacja = (ewakuacja ? 1 : 0);
                            send_msg(msg_id_kierowca, &msg);

                            if (ewakuacja)
                            {
                                break;
                            }

                            continue;
                        }

                        //Aktualizacja liczby aut w serwisie
                        sem_unlock(SEM_STATUS);

                        sem_lock(SEM_LICZNIKI);
                        shared->auta_w_serwisie++;
                        sem_unlock(SEM_LICZNIKI);

                        //Znajdowanie wolnego stanowiska
                        int mechanik_id = -1;

                        while (mechanik_id == -1)
                        {
                            if (ewakuacja)
                            {
                                break;
                            }

                            sem_lock(SEM_STATUS);
                            int otwarte_po = shared->serwis_otwarty;
                            sem_unlock(SEM_STATUS);
                            if (!otwarte_po)
                            {
                                //Serwis zamknięty w trakcie oczekiwania
                                msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                                msg.samochod.koszt = 0;
                                msg.samochod.czas_naprawy = 0;
                                msg.samochod.dodatkowa_usterka = 0;
                                msg.samochod.ewakuacja = 0;
                                send_msg(msg_id_kierowca, &msg);

                                sem_lock(SEM_LICZNIKI);
                                if (shared->auta_w_serwisie > 0)
                                {
                                    shared->auta_w_serwisie--;
                                }
                                sem_unlock(SEM_LICZNIKI);
                                break;
                            }

                            mechanik_id = znajdz_wolne_stanowisko(msg.samochod.marka);

                            if (mechanik_id == -1)
                            {
                                Msg wolny;
                                int r = recv_msg(msg_id_mechanik, &wolny, MSG_WOLNY_MECHANIK, IPC_NOWAIT);
                                if (r == -2)
                                {
                                    exit(0);
                                }
                                if (r == -1 && errno == EINTR)
                                {
                                    continue;
                                }

                                safe_wait_seconds(0.1);
                            }
                        }

                        if (ewakuacja)
                        {
                            break;
                        }

                        if (mechanik_id == -1)
                        {
                            continue;
                        }
                        
                        msg.samochod.id_stanowiska_roboczego = mechanik_id;

                        //Przekazanie auta do mechanika
                        msg.mtype = 100 + mechanik_id;
                        send_msg(msg_id_mechanik, &msg);
                        printf("[PRACOWNIK SERWISU %d] Przekazano auto %d do mechanika %d\n", id_pracownika, msg.samochod.pid_kierowcy, mechanik_id);
                        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Przekazano auto %d do mechanika %d", id_pracownika, msg.samochod.pid_kierowcy, mechanik_id);
                        zapisz_log(buffer);
                    }
                    else
                    {
                        printf("[PRACOWNIK SERWISU %d] Kierowca %d odrzucił usługę\n", id_pracownika, msg.samochod.pid_kierowcy);
                        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Kierowca %d odrzucił usługę", id_pracownika, msg.samochod.pid_kierowcy);
                        zapisz_log(buffer);
                    }
                    
                    continue;
                }
            }

            //Zamykanie zmiany jeśli serwis jest zamknięty i brak aktywnych mechaników
            if (!otwarte)
            {
                int wypisani = 0;
                while (recv_msg(msg_id_kierowca, &msg, MSG_REJESTRACJA, IPC_NOWAIT) != -1)
                {
                    printf("[PRACOWNIK SERWISU %d] Serwis zamknięty. Odsyłam kierowcę %d\n", id_pracownika, msg.samochod.pid_kierowcy);
                    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Serwis zamknięty. Odsyłam kierowcę %d", id_pracownika, msg.samochod.pid_kierowcy);
                    zapisz_log(buffer);

                    msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                    msg.samochod.koszt = 0;
                    msg.samochod.czas_naprawy = 0;
                    msg.samochod.dodatkowa_usterka = 0;
                    send_msg(msg_id_kierowca, &msg);
                    wypisani++;
                }

                if (wypisani > 0)
                {
                    sem_lock(SEM_LICZNIKI);
                    shared->liczba_oczekujacych_klientow = 0;
                    sem_unlock(SEM_LICZNIKI);

                    odebrano = 1;
                    continue;
                }

                if (!odebrano && !aktywni_mechanicy())
                {
                    //Ostatnie potwierdzenia płatności do przekazania kierowcom
                    if (recv_msg(msg_id_kasjer, &msg, MSG_POTWIERDZENIE_PLATNOSCI(id_pracownika), IPC_NOWAIT) != -1)
                    {
                        msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                        if (send_msg(msg_id_kierowca, &msg) == -1)
                        {
                            perror("[PRACOWNIK SERWISU] Błąd wysłania potwierdzenia płatności do kierowcy");
                        }
                        continue;
                    }

                    //Ostatnie sprawdzenie czy nie ma zaległych komunikatów
                    if (recv_msg(msg_id_mechanik, &msg, MSG_OD_MECHANIKA, IPC_NOWAIT) == -1)
                    {
                        printf("[PRACOWNIK SERWISU %d] Serwis zamknięty i wszystkie prace wykonane. Koniec zmiany\n", id_pracownika);
                        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Serwis zamknięty i wszystkie prace wykonane. Koniec zmiany", id_pracownika);
                        zapisz_log(buffer);
                        break;
                    }
                    else
                    {
                        //Jest komunikat do obsłużenia
                        send_msg(msg_id_mechanik, &msg);
                        continue;
                    }
                }
            }
            
            if (!odebrano)
            {
                //Brak komunikatów w tej iteracji
            }
        }

        printf("[PRACOWNIK SERWISU %d] Koniec zmiany. Czekam na kolejny dzień...\n", id_pracownika);
        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Koniec zmiany. Czekam na kolejny dzień...", id_pracownika);
        zapisz_log(buffer);
    }
    return 0;
}