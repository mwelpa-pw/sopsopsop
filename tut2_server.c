#include <stdio.h>      // Standardowe wejście/wyjście (printf, scanf, operacje na plikach)
#include <stdlib.h>     // Zarządzanie pamięcią (malloc, free), konwersje i kontrola procesu 
#include <fcntl.h>      // Kontrola plików i flagi dostępu (stałe O_*)
#include <sys/mman.h>   // Mapowanie pamięci i pamięć współdzielona (mmap, shm_open)
#include <sys/stat.h>   // Informacje o stanie plików i stałe uprawnień (np. S_IRUSR)
#include <unistd.h>     // Wywołania systemowe POSIX (fork, close, ftruncate, read, write)
#include <sys/wait.h>   // Funkcje oczekiwania na zakończenie procesu (wait, waitpid)
#include <string.h>     // Manipulacja ciągami znaków (sprintf, strcpy, strlen)
#include <pthread.h>    // Obsługa wątków standardu POSIX (tworzenie, muteksy, warunki)
#include <errno.h>      // Definicje numerów błędów systemowych i zmienna errno
#include <signal.h>     // Obsługa sygnałów systemowych (kill, signal, sigaction)
#include <time.h>       // Funkcje czasu i daty (time, nanosleep, struktura timespec)

/*
int shm_open(const char *name, int oflag, mode_t mode);
oflag - O-RDWR, O_RDONLY, O_RDWR, O_CREAT

*/
volatile sig_atomic_t keep_running = 1;

void handle_sigint(int sig) {
    keep_running = 0;
}

// Zadanie: Napisz klient server po RAMie.
void usage() {
    printf("3 < N <= 20");
    exit(0);
}

typedef struct {
    int row;
    int col;
} Pos;

int getIndex(int row, int col, int N) {
return N * row + col;
}

Pos getRowCol(int index, int N) {
    Pos p;
    p.row = index / N;
    p.col = index % N;
    return p;
}

typedef struct {
    pthread_mutex_t mutex;
    int N;
    int board[];
} shared_t;

void printBoard(int b[], int n, pthread_mutex_t *mutex) {
    pthread_mutex_lock(mutex);
    printf("\n\tboard state:\n");
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
            {
                printf("[%d]", b[i * n + j]);
            }
        printf("\n");
    }
    pthread_mutex_unlock(mutex);
}


int main(int argc, char** argv) {
    int n;
    ssize_t size = 2048;
    if (argc != 2) usage();

    n = atoi(argv[1]);
    if (n <= 3 || n > 20) usage();

    int pid = getpid();
    printf("My PID is: %d", pid);

    // shm
    char shm_name[64];
    int shm_fd;
    snprintf(shm_name, sizeof(shm_name), "/%d-board", pid);

    if ((shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666)) == -1)
        perror("shm_open");

    if (ftruncate(shm_fd, size) == -1) perror("ftruncate");

    shared_t *s = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    s->N = n;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(&(s->mutex), &attr);

    srand(time(NULL));

    // wypelnienie tablicy
    pthread_mutex_lock(&(s->mutex));

    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) 
            s->board[i * n + j] = rand() % 9 + 1;

    pthread_mutex_unlock(&(s->mutex));

    // signal handler registation
    signal(SIGINT, handle_sigint);

    while (keep_running) {
        sleep(3);
        printBoard(s->board, s->N, &(s->mutex));
    }

    // end
    printBoard(s->board, s->N, &(s->mutex));
    pthread_mutex_destroy(&(s->mutex));
    munmap(s, size);
    close(shm_fd);
    shm_unlink(shm_name);
    exit(0);
}