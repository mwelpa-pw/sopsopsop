#include <stdio.h>      // Standardowe wejście/wyjście (printf, scanf, operacje na plikach)
#include <stdlib.h>     // Zarządzanie pamięcią (malloc, free), konwersje i kontrola procesu 
#include <fcntl.h>      // Kontrola plików i flagi dostępu (stałe O_*)
#include <sys/mman.h>   // Mapowanie pamięci i pamięć współdzielona (mmap, shm_open)
#include <unistd.h>     // Wywołania systemowe POSIX (fork, close, ftruncate, read, write)
#include <string.h>     // Manipulacja ciągami znaków (sprintf, strcpy, strlen)
#include <pthread.h>    // Obsługa wątków standardu POSIX (tworzenie, muteksy, warunki)
#include <errno.h>      // Definicje numerów błędów systemowych i zmienna errno
#include <signal.h>     // Obsługa sygnałów systemowych (kill, signal, sigaction)
#include <time.h>       // Funkcje czasu i daty (time, nanosleep, struktura timespec)

typedef struct {
    pthread_mutex_t mutex;
    int N;
    int board[];
} shared_t;

int main(int argc, char **argv) {
    if (argc != 2) exit(1);
    int running = 1;

    int x, y, r, points = 0;
    int serversPid = atoi(argv[1]);
    int shm_fd;
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "/%d-board", serversPid);
    if ((shm_fd = shm_open(shm_name, O_RDWR, 0666)) == -1)
        perror("shm_open");

    shared_t *s = (shared_t *)mmap(0, 2048, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    int n = s->N;

    srand(time(NULL));

    while (running) {
        r = rand() % 9 + 1;

        if (r == 1) {
            printf("Oops...");

            munmap(s, 2048);
            close(shm_fd);
            exit(0);
        }

        x = rand() % n;
        y = rand() % n;

        printf("Trying to search field (%d, %d)\n", x, y);
        
        if (pthread_mutex_lock(&(s->mutex)) < 0 && errno == EOWNERDEAD)
            pthread_mutex_consistent(&(s->mutex));

        int p = s->board[y * n + x];
        if (p != 0) {
            printf("found %d points\n", p);
            s->board[y * n + x] = 0;
            points += p;
        } else {
            printf("GAME OVER: score %d\n", points);
            running = 0;
        }
        pthread_mutex_unlock(&(s->mutex));
        sleep(1);
    }

    munmap(s, 2048);
    close(shm_fd);
    exit(EXIT_SUCCESS);
}