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
    Zadanie 1
    program zliczajacy wystapienia pojedynczego znaku w zadanym pliku (char).
*/
void error(char* message) {
    printf("%s", message);
    exit(EXIT_FAILURE);
}

typedef struct {
    int charsmap[256];
    pthread_mutex_t mutex;
} histogram_t;

int main (int argc, char** argv) {
    // --- 1 ---
    // Otwarcie pliku
    if (argc != 2) error("argc");

    int N = atoi(argv[1]);

    const char *filename = "log.txt";
    // pobranie informacji o pliku
    struct stat file_info;
    if (stat(filename, &file_info) == -1) error("stat");
    size_t sz = file_info.st_size;
    if (sz == 0) { 
        // Plik jest pusty. Usunięto close(), bo plik nie został tu otwarty!
        printf("Plik jest pusty, konczymy dzialanie...\n");
        return 0;
    }

    histogram_t *mapgram = (histogram_t *)mmap(NULL, sizeof(histogram_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0); // mallokujemy
    if (mapgram == MAP_FAILED) error("mmap");

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(&(mapgram->mutex), &attr);

    int part_sz = sz / N;

    for (int i = 0; i < N; i++) {
        int pid = fork();
        if (pid == 0) {
            // kid
            int file_fd = open(filename, O_CREAT | O_RDWR, 0666);
            int start_index = part_sz * i;
            int stop_index = (i + 1) * part_sz;
            int chars[256] = {0};

            char* file_mapped = mmap(NULL, sz, PROT_READ,  MAP_PRIVATE, file_fd, 0);
            for (int j = start_index; j < stop_index; j++) {
                chars[(unsigned char)file_mapped[j]] += 1; // domyslnie char = [-128; 127]
            }

            srand(time(NULL) ^ getpid());
            // if (rand() % 100 < 3) {
            //     abort(); // BUM! Dziecko ginie z zablokowanym mutexem.
            // }

            if (pthread_mutex_lock(&(mapgram->mutex)) < 0 && errno == EOWNERDEAD)
                pthread_mutex_consistent(&(mapgram->mutex));

            for (int j = 0; j < 256; j++)
                mapgram->charsmap[j] += chars[j];

            pthread_mutex_unlock(&(mapgram->mutex));

            munmap(file_mapped, sz);
            close(file_fd);
            exit(0);
        } else if (pid > 0) {
            // parent

        } else error("fork");
    }



    // koniec
    int status;
    int success = 1; // 1 - ok, 0 nie ok
    for (int i = 0; i < N; i++) {
        wait(&status); // zamiast wait(NULL);
        if (WIFSIGNALED(status)) success = 0;
    }

    if (success)
        for (int i = 0; i < 256; i++)
            printf("%c: %d\n", i, mapgram->charsmap[i]);
    else printf("sth has been aborted");

    munmap(mapgram, sizeof(histogram_t));
    exit(EXIT_SUCCESS);
}