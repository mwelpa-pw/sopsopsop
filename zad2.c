#define _XOPEN_SOURCE 700 // Wymagane dla PTHREAD_MUTEX_ROBUST (musi być na samej górze!)

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h> // Wymagane dla semaforów nazwanych (sem_open, sem_wait)

// Stałe nazwy dla naszych zasobów w systemie operacyjnym
const char *SHM_NAME = "/moje_shm_liczniki";
const char *SEM_NAME = "/moj_sem_startowy";

void error(char* message) {
    perror(message); // Używam perror, żeby w razie błędu wypisało dokładny powód z systemu!
    exit(EXIT_FAILURE);
}

// Struktura, która trafi do pamięci dzielonej
typedef struct {
    int process_count;       // Licznik współpracujących procesów
    pthread_mutex_t mutex;   // Współdzielony mutex chroniący licznik
} shared_data_t;


int main() {
    // ========================================================================
    // KROK 1: SEMAFOR NAZWANY (Ochrona przed wyścigiem przy inicjalizacji)
    // ========================================================================
    // Tworzymy lub otwieramy semafor nazwany. Wartość początkowa to 1 (otwarty).
    // O_CREAT sprawi, że pierwszy proces go stworzy, a kolejne po prostu otworzą.
    sem_t *init_sem = sem_open(SEM_NAME, O_CREAT, 0666, 1); // TA OPERACJE JEST ATOMOWA!!!
    // inne procesy zignoruja sem_open jesli juz istnieje
    if (init_sem == SEM_FAILED) error("sem_open");

    // Zamykamy semafor (wartość 0). Pierwszy proces wchodzi bez czekania.
    // Jeśli kilka procesów wystartuje w tej samej mikrosekundzie, reszta tu utknie
    // i poczeka, aż pierwszy proces stworzy pamięć i zainicjuje mutex!
    sem_wait(init_sem);

    // ========================================================================
    // KROK 2: PAMIĘĆ DZIELONA (Tworzenie i mapowanie)
    // ========================================================================
    // Używamy shm_open zamiast open, bo to nazwana pamięć dzielona (w RAM-ie)
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) error("shm_open");

    // Sprawdzamy, czy pamięć jest nowa (rozmiar 0), czy już zainicjalizowana
    struct stat shm_info;
    if (fstat(shm_fd, &shm_info) == -1) error("fstat");
    size_t sz = shm_info.st_size;

    // Flagowa zmienna pomocnicza do późniejszych logów
    int is_creator = 0; 

    if (sz == 0) {
        // Skoro rozmiar to 0, TEN proces jest stwórcą (był pierwszy)
        is_creator = 1;
        
        // Rozciągamy pamięć do rozmiaru naszej struktury
        if (ftruncate(shm_fd, sizeof(shared_data_t)) == -1) error("ftruncate");
    }

    // Mapujemy pamięć (nie używamy MAP_ANONYMOUS, bo mamy deskryptor shm_fd!)
    shared_data_t *shared_data = (shared_data_t *)mmap(NULL, sizeof(shared_data_t), 
                                  PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) error("mmap");
    close(shm_fd); // Deskryptor można zamknąć, mapowanie zostaje

    // ========================================================================
    // KROK 3: INICJALIZACJA STRUKTURY (Tylko dla twórcy)
    // ========================================================================
    if (is_creator) {
        shared_data->process_count = 0;

        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); // Dla procesów
        pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);    // Odporny na śmierć
        
        pthread_mutex_init(&(shared_data->mutex), &attr);
        pthread_mutexattr_destroy(&attr);
    }

    // Otwieramy semafor! Inicjalizacja zakończona. 
    // Od teraz oczekujące procesy mogą bezpiecznie przejść przez `sem_wait` u góry.
    sem_post(init_sem);

    // ========================================================================
    // KROK 4: INKREMENTACJA LICZNIKA I DZIAŁANIE (Sekcja krytyczna)
    // ========================================================================
    // Bezpiecznie blokujemy mutex (wraz z obsługą ewentualnej śmierci poprzednika)
    int rc = pthread_mutex_lock(&(shared_data->mutex));
    if (rc == EOWNERDEAD) {
        printf("[Ostrzeżenie] Poprzedni proces umarł z mutexem. Naprawiam...\n");
        pthread_mutex_consistent(&(shared_data->mutex));
    }

    // Zwiększamy licznik (bo właśnie się podłączyliśmy)
    shared_data->process_count++;
    
    // Zapisujemy stan do lokalnej zmiennej, żeby wypisać go POZA sekcją krytyczną
    int current_count = shared_data->process_count;

    pthread_mutex_unlock(&(shared_data->mutex));

    // Wypisanie ilości procesów i sen (zgodnie z poleceniem)
    printf("Ilość współpracujących procesów: %d\n", current_count);
    sleep(2);

    // ========================================================================
    // KROK 5: DEKREMENTACJA I ROZŁĄCZANIE (Sprzątanie)
    // ========================================================================
    rc = pthread_mutex_lock(&(shared_data->mutex));
    if (rc == EOWNERDEAD) pthread_mutex_consistent(&(shared_data->mutex));

    // Odłączamy się, więc zmniejszamy licznik
    shared_data->process_count--;
    int remaining_count = shared_data->process_count; // Sprawdzamy, ilu zostało

    pthread_mutex_unlock(&(shared_data->mutex));

    // Odpinamy pamięć z procesu
    if (munmap(shared_data, sizeof(shared_data_t)) == -1) error("munmap");

    // Jeśli licznik spadł do zera, to my jesteśmy ostatnim procesem!
    // My gasimy światło w systemie operacyjnym.
    if (remaining_count == 0) {
        printf("Jestem ostatnim procesem. Niszcze obiekty IPC...\n");
        shm_unlink(SHM_NAME); // Niszczy nazwaną pamięć dzieloną
        sem_unlink(SEM_NAME); // Niszczy nazwany semafor
    }

    return 0;
}