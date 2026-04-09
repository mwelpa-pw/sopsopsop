/*
=== mmap ===
void *mmap(void *addr, size_t len, int prot, int flags,
           int fildes, off_t off);

addr - jaki obszar (NULL)
len - dlugosc
prot - one or more of the following flags:

       PROT_EXEC  Pages may be executed.

       PROT_READ  Pages may be read.

       PROT_WRITE Pages may be written.

       PROT_NONE  Pages may not be accessed.

flags - MAP_SHARED/MAP_PRIVATE/MAP_ANONYMOUS (gdy chcemy stworzyc nowy obszar, a nie
     na podstawie czegos) wtedy fd = -1
filesdes - deskryptor
offset - kursor od kiedy brac strone do pamieci

--- Funkcje ---
msync - flush
munmap - unmap
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>    // Do funkcji open() i flag O_RDWR
#include <math.h>
#include <time.h>

// Zadanie: Napisz program obliczajacy liczbe PI met. Monte Carlo.
void usage() {
    printf("0 < N < 30");
    exit(0);
}

/*
Monte Carlo method
*/
typedef struct {
    char *log;
    int *calc;
} fd_struct;

void MCM(fd_struct *s, int offset) {
    srand(time(NULL) ^ getpid());
    int circle_points = 0;
    double rand_x, rand_y;

    for (int i = 0; i < 100000; i++) {
        rand_x = (double)rand() / RAND_MAX;
        rand_y = (double)rand() / RAND_MAX;

        if ((rand_x * rand_x) + (rand_y * rand_y) <= 1)
            circle_points += 1;
    }

    s->calc[4 * offset] = circle_points;
    char temp_buff[9];
    sprintf(temp_buff, "%07d\n", circle_points); // (gdzie, jak-format, co)
    memcpy(s->log + 8 * offset, temp_buff, 8); // (gdzie, skad, ile)

    exit(0);
}

int main(int argc, char **argv) {
    if (argc != 2) usage();
    int n = atoi(argv[1]);
    if (n <= 0 || n >= 30) usage();

    // mapping
    // calculations
    int *fd_calc = (int *)mmap(NULL, n * 4, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    // logging to file
    const char *logfile = "log.txt";
    int fd_logfile = open(logfile, O_RDWR | O_CREAT, 0666);

    if (fd_logfile < 0) perror("fd logfile");
    if (ftruncate(fd_logfile, n*8) == -1) perror("truncate"); 

    char* fd_log = (char *)mmap(NULL, n * 8, PROT_READ | PROT_WRITE, MAP_SHARED, fd_logfile, 0);

    // structure
    fd_struct *s = malloc(sizeof(fd_struct));
    s->calc = fd_calc;
    s->log = fd_log;

    int offset = 0;
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // kid
            MCM(s, i);
        } else if (pid > 0) {
            // parent
            offset += 8;
        } else {
            perror("fork");
        }
    }

    for (int i = 0; i < n; i++) {
        wait(NULL); 
    }

    // cleanup
    free(s);
    munmap(fd_calc, n * 4);
    munmap(fd_log, n * 8);
    close(fd_logfile);
    
    exit(0);
}