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
#include <pthread.h>

//Progi do otwierania dodatkowych okienek
#define K1 3
#define K2 5

typedef struct
{
    Msg msg;
    int id_pracownika;
} KlientTask;

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
    if (!aktywni)
    {
        sem_lock(SEM_LICZNIKI);
        if (shared->liczba_oczekujacych_klientow > 0)
        {
            aktywni = 1;
        }
        sem_unlock(SEM_LICZNIKI);
    }
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

static void *obsluz_klienta(void *arg)
{
    KlientTask *task = (KlientTask *)arg;
    Msg msg = task->msg;
    int id_pracownika = task->id_pracownika;
    free(task);

    char buffer[256];
    pid_t pid = msg.samochod.pid_kierowcy;
    int auto_w_serwisie = 0;
    int oddane_mechanikowi = 0;

    msg.samochod.id_pracownika = id_pracownika;

    if (!marka_obslugiwana(msg.samochod.marka))
    {
        printf("[PRACOWNIK SERWISU %d] Marka %s nieobsługiwana. Odsyłam kierowcę %d\n", id_pracownika, msg.samochod.marka, pid);
        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Marka %s nieobsługiwana. Odsyłam kierowcę %d", id_pracownika, msg.samochod.marka, pid);
        zapisz_log(buffer);

        msg.mtype = MSG_KIEROWCA(pid);
        msg.samochod.koszt = -1;
        msg.samochod.czas_naprawy = 0;
        msg.samochod.dodatkowa_usterka = 0;
        msg.samochod.ewakuacja = 0;
        send_msg(msg_id_kierowca, &msg);
        return NULL;
    }

    printf("[PRACOWNIK SERWISU %d] Obsługa kierowcy %d, marka %s, usługa ID: %d\n", id_pracownika, pid, msg.samochod.marka, msg.samochod.id_uslugi);
    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Obsługa kierowcy %d, marka %s, usługa ID: %d", id_pracownika, pid, msg.samochod.marka, msg.samochod.id_uslugi);
    zapisz_log(buffer);

    Usluga u = pobierz_usluge(msg.samochod.id_uslugi);
    msg.samochod.czas_naprawy = u.czas_wykonania;
    msg.samochod.koszt = u.koszt;
    msg.samochod.zaakceptowano = 0;
    msg.samochod.dodatkowa_usterka = 0;
    msg.samochod.id_stanowiska_roboczego = -1;

    printf("[PRACOWNIK SERWISU %d] Wycena dla kierowcy %d: koszt %d, czas naprawy %d\n", id_pracownika, pid, msg.samochod.koszt, msg.samochod.czas_naprawy);
    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Wycena dla kierowcy %d: koszt %d, czas naprawy %d", id_pracownika, pid, msg.samochod.koszt, msg.samochod.czas_naprawy);
    zapisz_log(buffer);

    msg.mtype = MSG_KIEROWCA(pid);
    send_msg(msg_id_kierowca, &msg);

    //Oczekiwanie na decyzję kierowcy
    Msg decyzja;
    while (1)
    {
        if (ewakuacja)
        {
            msg.mtype = MSG_KIEROWCA(pid);
            msg.samochod.ewakuacja = 1;
            msg.samochod.koszt = 0;
            msg.samochod.czas_naprawy = 0;
            msg.samochod.dodatkowa_usterka = 0;
            send_msg(msg_id_kierowca, &msg);
            return NULL;
        }

        sem_lock(SEM_STATUS);
        int otwarte = shared->serwis_otwarty;
        sem_unlock(SEM_STATUS);
        if (!otwarte)
        {
            msg.mtype = MSG_KIEROWCA(pid);
            msg.samochod.koszt = 0;
            msg.samochod.czas_naprawy = 0;
            msg.samochod.dodatkowa_usterka = 0;
            msg.samochod.ewakuacja = 0;
            send_msg(msg_id_kierowca, &msg);
            return NULL;
        }

        int r = recv_msg(msg_id_kierowca, &decyzja, MSG_DECYZJA_USLUGI_PID(pid), IPC_NOWAIT);
        if (r == -2)
        {
            return NULL;
        }
        if (r == 0)
        {
            msg = decyzja;
            break;
        }
        if (errno == EINTR)
        {
            continue;
        }
    }

    if (!msg.samochod.zaakceptowano)
    {
        printf("[PRACOWNIK SERWISU %d] Kierowca %d odrzucił usługę\n", id_pracownika, pid);
        snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Kierowca %d odrzucił usługę", id_pracownika, pid);
        zapisz_log(buffer);
        return NULL;
    }

    sem_lock(SEM_LICZNIKI);
    shared->auta_w_serwisie++;
    sem_unlock(SEM_LICZNIKI);
    auto_w_serwisie = 1;

    //Znajdowanie wolnego stanowiska
    int mechanik_id = -1;
assign_mechanic:
    mechanik_id = -1;
    while (mechanik_id == -1)
    {
        if (ewakuacja)
        {
            if (auto_w_serwisie && !oddane_mechanikowi)
            {
                sem_lock(SEM_LICZNIKI);
                if (shared->auta_w_serwisie > 0)
                {
                    shared->auta_w_serwisie--;
                }
                sem_unlock(SEM_LICZNIKI);
            }
            return NULL;
        }

        sem_lock(SEM_STATUS);
        int otwarte_po = shared->serwis_otwarty;
        sem_unlock(SEM_STATUS);
        if (!otwarte_po)
        {
            msg.mtype = MSG_KIEROWCA(pid);
            msg.samochod.koszt = 0;
            msg.samochod.czas_naprawy = 0;
            msg.samochod.dodatkowa_usterka = 0;
            msg.samochod.ewakuacja = 0;
            send_msg(msg_id_kierowca, &msg);

            if (auto_w_serwisie && !oddane_mechanikowi)
            {
                sem_lock(SEM_LICZNIKI);
                if (shared->auta_w_serwisie > 0)
                {
                    shared->auta_w_serwisie--;
                }
                sem_unlock(SEM_LICZNIKI);
            }

            return NULL;
        }

        mechanik_id = znajdz_wolne_stanowisko(msg.samochod.marka);
        if (mechanik_id == -1)
        {
            Msg wolny;
            int r = recv_msg(msg_id_mechanik, &wolny, MSG_WOLNY_MECHANIK, IPC_NOWAIT);
            if (r == -2)
            {
                return NULL;
            }
            if (r == -1 && errno == EINTR)
            {
                continue;
            }
        }
    }

    msg.samochod.id_stanowiska_roboczego = mechanik_id;
    msg.mtype = 100 + mechanik_id;
    send_msg(msg_id_mechanik, &msg);
    oddane_mechanikowi = 1;

    printf("[PRACOWNIK SERWISU %d] Przekazano auto %d do mechanika %d\n", id_pracownika, pid, mechanik_id);
    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Przekazano auto %d do mechanika %d", id_pracownika, pid, mechanik_id);
    zapisz_log(buffer);

    //Obsługa informacji od mechanika i płatności
    while (1)
    {
        if (ewakuacja)
        {
            return NULL;
        }

        int r = recv_msg(msg_id_mechanik, &msg, MSG_MECHANIK_EVENT_PID(pid), IPC_NOWAIT);
        if (r == -2)
        {
            return NULL;
        }
        if (r == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            continue;
        }

        if (msg.samochod.id_stanowiska_roboczego < 0)
        {
            oddane_mechanikowi = 0;
            goto assign_mechanic;
        }

        if (msg.samochod.dodatkowa_usterka)
        {
            printf("[PRACOWNIK SERWISU %d] Mechanik ze stanowiska %d zgłosił dodatkową usterkę w aucie %d: %s\n", id_pracownika, msg.samochod.id_stanowiska_roboczego, pid, pobierz_usluge(msg.samochod.id_dodatkowej_uslugi).nazwa);
            snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Mechanik ze stanowiska %d zgłosił dodatkową usterkę w aucie %d: %s", id_pracownika, msg.samochod.id_stanowiska_roboczego, pid, pobierz_usluge(msg.samochod.id_dodatkowej_uslugi).nazwa);
            zapisz_log(buffer);

            msg.mtype = MSG_KIEROWCA(pid);
            send_msg(msg_id_kierowca, &msg);

            Msg decyzja_dod;
            while (1)
            {
                if (ewakuacja)
                {
                    return NULL;
                }

                int rd = recv_msg(msg_id_kierowca, &decyzja_dod, MSG_DECYZJA_DODATKOWA_PID(pid), IPC_NOWAIT);
                if (rd == -2)
                {
                    return NULL;
                }
                if (rd == 0)
                {
                    msg = decyzja_dod;
                    break;
                }
                if (errno == EINTR)
                {
                    continue;
                }
            }

            msg.mtype = 100 + msg.samochod.id_stanowiska_roboczego;
            send_msg(msg_id_mechanik, &msg);
            continue;
        }

        //Koniec naprawy - przekazanie do kasy
        int koszt_podstawowy = pobierz_usluge(msg.samochod.id_uslugi).koszt;
        int koszt_dodatkowy = (msg.samochod.dodatkowy_koszt > 0) ? msg.samochod.dodatkowy_koszt : 0;
        msg.samochod.koszt = koszt_podstawowy + koszt_dodatkowy;

        msg.mtype = MSG_KASA;
        send_msg(msg_id_kasjer, &msg);

        Msg potw;
        while (1)
        {
            if (ewakuacja)
            {
                return NULL;
            }

            int rp = recv_msg(msg_id_kasjer, &potw, MSG_POTWIERDZENIE_PLATNOSCI_PID(pid), IPC_NOWAIT);
            if (rp == -2)
            {
                return NULL;
            }
            if (rp == 0)
            {
                msg = potw;
                break;
            }
            if (errno == EINTR)
            {
                continue;
            }
        }

        msg.mtype = MSG_KIEROWCA(pid);
        send_msg(msg_id_kierowca, &msg);
        return NULL;
    }
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

    //Rejestracja PID pracownika
    sem_lock(SEM_STATUS);
    shared->pid_pracownik[id_pracownika] = getpid();
    sem_unlock(SEM_STATUS);

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
            printf("[PRACOWNIK SERWISU %d] Otrzymano sygnał pożaru! Uciekam!\n", id_pracownika);
            snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Otrzymano sygnał pożaru! Uciekam!", id_pracownika);
            zapisz_log(buffer);
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
            //Obsługa nowych klientów
            if (czy_aktywny && otwarte)
            {
                //Pobieramy zgłoszenie rejestracji
                int r = recv_msg(msg_id_kierowca, &msg, MSG_REJESTRACJA, IPC_NOWAIT);
                if (r == 0)
                {
                    odebrano = 1;

                    //Aktualizacja liczby oczekujących klientów
                    sem_lock(SEM_LICZNIKI);
                    if (shared->liczba_oczekujacych_klientow > 0)
                    {
                        shared->liczba_oczekujacych_klientow--;
                    }
                    sem_unlock(SEM_LICZNIKI);

                    KlientTask *task = malloc(sizeof(KlientTask));
                    if (!task)
                    {
                        perror("malloc KlientTask failed");
                        msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                        msg.samochod.koszt = 0;
                        msg.samochod.czas_naprawy = 0;
                        msg.samochod.dodatkowa_usterka = 0;
                        msg.samochod.ewakuacja = 0;
                        send_msg(msg_id_kierowca, &msg);
                        continue;
                    }

                    task->msg = msg;
                    task->id_pracownika = id_pracownika;

                    pthread_t tid;
                    if (pthread_create(&tid, NULL, obsluz_klienta, task) == 0)
                    {
                        pthread_detach(tid);
                    }
                    else
                    {
                        perror("pthread_create obsluz_klienta failed");
                        free(task);
                        msg.mtype = MSG_KIEROWCA(msg.samochod.pid_kierowcy);
                        msg.samochod.koszt = 0;
                        msg.samochod.czas_naprawy = 0;
                        msg.samochod.dodatkowa_usterka = 0;
                        msg.samochod.ewakuacja = 0;
                        send_msg(msg_id_kierowca, &msg);
                    }

                    continue;
                }
                if (r == -2)
                {
                    exit(0);
                }
            }

            //Zamykanie zmiany jeśli serwis jest zamknięty i brak aktywnych mechaników
            if (!otwarte)
            {
                int wypisani = 0;
                while (1)
                {
                    int r = recv_msg(msg_id_kierowca, &msg, MSG_REJESTRACJA, IPC_NOWAIT);
                    if (r == -2)
                    {
                        exit(0);
                    }
                    if (r != 0)
                    {
                        break;
                    }

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
                    printf("[PRACOWNIK SERWISU %d] Serwis zamknięty i wszystkie prace wykonane. Koniec zmiany\n", id_pracownika);
                    snprintf(buffer, sizeof(buffer), "[PRACOWNIK SERWISU %d] Serwis zamknięty i wszystkie prace wykonane. Koniec zmiany", id_pracownika);
                    zapisz_log(buffer);
                    break;
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