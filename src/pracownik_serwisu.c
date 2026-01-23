#define _DEFAULT_SOURCE

#include "serwis_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/msg.h>

//Progi do otwierania dodatkowych okienek
#define K1 3
#define K2 5

//Funkcja sprawdzająca długość kolejki oczekujących klientów
int sprawdz_dlugosc_kolejki()
{
    int liczba = 0;
    sem_lock(SEM_SHARED);
    liczba = shared->liczba_oczekujacych_klientow;
    sem_unlock(SEM_SHARED);
    return liczba;
}

//Funkcja znajdująca wolnego mechanika mogącego obsłużyć auto
int znajdz_wolne_stanowisko(const char *marka)
{
    int is_UY = (strcmp(marka, "U") == 0 || strcmp(marka, "Y") == 0);

    sem_lock(SEM_SHARED);
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
            sem_unlock(SEM_SHARED);
            return i;
        }
    }
    sem_unlock(SEM_SHARED);
    return -1;
}

//Funkcja sprawdzająca czy są aktywni mechanicy
int aktywni_mechanicy()
{
    int aktywni = 0;
    sem_lock(SEM_SHARED);
    for (int i = 0; i < MAX_STANOWISK; i++)
    {
        if (shared->stanowiska[i].zajete)
        {
            aktywni = 1;
            break;
        }
    }
    sem_unlock(SEM_SHARED);
    return aktywni;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Brak ID pracownika serwisu\n");
        exit(1);
    }

    int id_pracownika = atoi(argv[1]);

    //Dołączenie do IPC
    init_ipc(0);

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
        //Czekanie na otwarcie serwisu
        wait_serwis_otwarty();

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
            sem_lock(SEM_SHARED);
            int pozar = shared->pozar;
            int otwarte = shared->serwis_otwarty;
            sem_unlock(SEM_SHARED);

            if (pozar)
            {
                printf("[PRACOWNIK SERWISU %d] Pożar!\n", id_pracownika);
                snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Pożar!", id_pracownika);
                zapisz_log(buffer);
                break;
            }

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

            //Jeśli pracownik nie jest aktywny i serwis jest otwarty, to czeka
            if (!czy_aktywny && otwarte)
            {
                wait_nowa_wiadomosc(0);
                continue;
            }

            int odebrano = 0;

            //Odbiór komunikatów od mechaników
            if (recv_msg(msg_id, &msg, MSG_OD_MECHANIKA, IPC_NOWAIT) != -1)
            {
                odebrano = 1;

                if (msg.samochod.dodatkowa_usterka)
                {
                    //Mechanik zgłosił dodatkową usterkę
                    printf("[PRACOWNIK SERWISU %d] Mechanik ze stanowiska %d zgłosił dodatkową usterkę w aucie %d: %s\n", id_pracownika, msg.samochod.id_stanowiska_roboczego, msg.samochod.pid_kierowcy, pobierz_usluge(msg.samochod.id_dodatkowej_uslugi).nazwa);
                    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Mechanik ze stanowiska %d zgłosił dodatkową usterkę w aucie %d: %s", id_pracownika, msg.samochod.id_stanowiska_roboczego, msg.samochod.pid_kierowcy, pobierz_usluge(msg.samochod.id_dodatkowej_uslugi).nazwa);
                    zapisz_log(buffer);

                    msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                    send_msg(msg_id, &msg);
                    signal_nowa_wiadomosc();
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
                    send_msg(msg_id, &msg);
                    signal_nowa_wiadomosc();
                }

                continue;
            }

            //Odbiór potwierdzenia płatności z kasy i przekazanie do kierowcy
            if (recv_msg(msg_id, &msg, MSG_POTWIERDZENIE_PLATNOSCI(id_pracownika), IPC_NOWAIT) != -1)
            {
                odebrano = 1;
                msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                if (send_msg(msg_id, &msg) == -1)
                {
                    perror("[PRACOWNIK SERWISU] Błąd wysłania potwierdzenia płatności do kierowcy");
                }
                else
                {
                    signal_nowa_wiadomosc();
                }
                continue;
            }

            //Odbiór decyzji o dodatkowej usterce od kierowcy
            if (recv_msg(msg_id, &msg, MSG_DECYZJA_DODATKOWA(id_pracownika), IPC_NOWAIT) != -1)
            {
                odebrano = 1;
                printf("[PRACOWNIK SERWISU %d] Otrzymano decyzję od kierowcy %d. Przekazuję mechanikowi na stanowisko %d\n", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.id_stanowiska_roboczego);
                snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Otrzymano decyzję od kierowcy %d. Przekazuję mechanikowi na stanowisko %d", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.id_stanowiska_roboczego);
                zapisz_log(buffer);

                //Przekazanie decyzji mechanikowi
                msg.mtype = 100 + msg.samochod.id_stanowiska_roboczego;
                send_msg(msg_id, &msg);
                signal_nowa_wiadomosc();
                
                continue;
            }


            //Obsługa nowych klientów
            if (czy_aktywny && otwarte)
            {
                //Pobieramy zgłoszenie rejestracji
                if (recv_msg(msg_id, &msg, MSG_REJESTRACJA, IPC_NOWAIT) != -1)
                {
                    odebrano = 1;

                    msg.samochod.id_pracownika = id_pracownika;

                    //Aktualizacja liczby oczekujących klientów
                    sem_lock(SEM_SHARED);
                    if (shared->liczba_oczekujacych_klientow > 0)
                        shared->liczba_oczekujacych_klientow--;
                    sem_unlock(SEM_SHARED);

                    printf("[PRACOWNIK SERWISU %d] Obsługa kierowcy %d, marka %s, usługa ID: %d\n", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.marka, msg.samochod.id_uslugi);
                    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Obsługa kierowcy %d, marka %s, usługa ID: %d", id_pracownika, msg.samochod.pid_kierowcy, msg.samochod.marka, msg.samochod.id_uslugi);
                    zapisz_log(buffer);

                    //Symulacja czasu obsługi przy rejestracji
                    //sleep(1);
                    safe_wait_seconds(1);

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
                    send_msg(msg_id, &msg);
                    signal_nowa_wiadomosc();

                    //Oczekiwanie na decyzję kierowcy
                    Msg decyzja;
                    int odebrano_decyzje = 0;

                    while (1)
                    {
                        sem_lock(SEM_SHARED);
                        if (shared->pozar)
                        {
                            sem_unlock(SEM_SHARED);
                            printf("[PRACOWNIK SERWISU %d] Pożar! Anuluję obsługę klienta %d\n", id_pracownika, msg.samochod.pid_kierowcy);
                            snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Pożar! Anuluję obsługę klienta %d", id_pracownika, msg.samochod.pid_kierowcy);
                            zapisz_log(buffer);
                            
                            odebrano_decyzje = -1;
                            break;
                        }
                        sem_unlock(SEM_SHARED);

                        if (recv_msg(msg_id, &decyzja, MSG_DECYZJA_USLUGI(id_pracownika), IPC_NOWAIT) != -1)
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
                                send_msg(msg_id, &decyzja);
                                signal_nowa_wiadomosc();
                            }
                        }

                        wait_nowa_wiadomosc(0);
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

                        sem_lock(SEM_SHARED);
                        if (!shared->serwis_otwarty || shared->pozar)
                        {
                            sem_unlock(SEM_SHARED);
                            printf("[PRACOWNIK SERWISU %d] Serwis właśnie zamknięto. Odsyłam kierowcę %d\n", id_pracownika, msg.samochod.pid_kierowcy);
                            snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Serwis właśnie zamknięto. Odsyłam kierowcę %d", id_pracownika, msg.samochod.pid_kierowcy);
                            zapisz_log(buffer);

                            msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                            msg.samochod.koszt = 0;
                            send_msg(msg_id, &msg);
                            signal_nowa_wiadomosc();
                            continue;
                        }

                        //Aktualizacja liczby aut w serwisie
                        shared->auta_w_serwisie++;
                        sem_unlock(SEM_SHARED);

                        //Znajdowanie wolnego stanowiska
                        int mechanik_id = -1;
                        int poinforowano = 0;

                        while (mechanik_id == -1)
                        {
                            sem_lock(SEM_SHARED);
                            if (shared->pozar)
                            {
                                sem_unlock(SEM_SHARED);
                                printf("[PRACOWNIK SERWISU %d] Pożar! Anuluję obsługę klienta %d\n", id_pracownika, msg.samochod.pid_kierowcy);
                                snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Pożar! Anuluję obsługę klienta %d", id_pracownika, msg.samochod.pid_kierowcy);
                                zapisz_log(buffer);
                                
                                break;
                            }
                            sem_unlock(SEM_SHARED);

                            mechanik_id = znajdz_wolne_stanowisko(msg.samochod.marka);

                            if (mechanik_id == -1)
                            {
                                if (!poinforowano)
                                {
                                    printf("[PRACOWNIK SERWISU %d] Brak wolnych mechaników dla %s, czekam...\n", id_pracownika, msg.samochod.marka);
                                    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Brak wolnych mechaników dla %s, czekam...", id_pracownika, msg.samochod.marka);
                                    zapisz_log(buffer);

                                    poinforowano = 1;
                                }
                                
                                wait_wolny_mechanik();
                            }
                        }

                        if (mechanik_id == -1)
                        {
                            break;
                        }
                        
                        msg.samochod.id_stanowiska_roboczego = mechanik_id;

                        //Przekazanie auta do mechanika
                        msg.mtype = 100 + mechanik_id;
                        send_msg(msg_id, &msg);
                        signal_nowa_wiadomosc();
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
                if (recv_msg(msg_id, &msg, MSG_REJESTRACJA, IPC_NOWAIT) != -1)
                {
                    printf("[PRACOWNIK SERWISU %d] Serwis zamknięty. Odsyłam kierowcę %d\n", id_pracownika, msg.samochod.pid_kierowcy);
                    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Serwis zamknięty. Odsyłam kierowcę %d", id_pracownika, msg.samochod.pid_kierowcy);
                    zapisz_log(buffer);

                    msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                    msg.samochod.koszt = 0;
                    send_msg(msg_id, &msg);
                    signal_nowa_wiadomosc();

                    sem_lock(SEM_SHARED);
                    if (shared->liczba_oczekujacych_klientow > 0)
                        shared->liczba_oczekujacych_klientow--;
                    sem_unlock(SEM_SHARED);

                    odebrano = 1;
                    continue;
                }

                if (!odebrano && !aktywni_mechanicy())
                {
                    //Ostatnie sprawdzenie czy nie ma zaległych komunikatów
                    if (recv_msg(msg_id, &msg, MSG_OD_MECHANIKA, IPC_NOWAIT) == -1)
                    {
                        printf("[PRACOWNIK SERWISU %d] Serwis zamknięty i wszystkie prace wykonane. Koniec zmiany\n", id_pracownika);
                        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Serwis zamknięty i wszystkie prace wykonane. Koniec zmiany", id_pracownika);
                        zapisz_log(buffer);
                        break;
                    }
                    else
                    {
                        //Jest komunikat do obsłużenia
                        send_msg(msg_id, &msg);
                        continue;
                    }
                }
            }
            
            if (!odebrano)
            {
                wait_nowa_wiadomosc(0);
            }
        }

        printf("[PRACOWNIK SERWISU %d] Koniec zmiany. Czekam na kolejny dzień...\n", id_pracownika);
        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Koniec zmiany. Czekam na kolejny dzień...", id_pracownika);
        zapisz_log(buffer);
    }
    return 0;
}