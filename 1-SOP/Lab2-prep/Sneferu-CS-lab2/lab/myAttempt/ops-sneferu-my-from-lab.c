#define _GNU_SOURCE
#include <bits/types/siginfo_t.h>
#include <bits/types/sigset_t.h>
#include <bits/types/struct_iovec.h>
// #include <cerrno>
// #include <csignal>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t last_sig;

ssize_t bulk_read(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (c == 0)
            return len;  // EOF
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

ssize_t bulk_write(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

void usage(int argc, char* argv[])
{
    printf("%s p k \n", argv[0]);
    printf("\tp - path to file with stones\n");
    printf("\t0 < k < 8 - number of child processes\n");
    exit(EXIT_FAILURE);
}

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void sighandler(int SigNo) { last_sig = SigNo; }

void child_work(char* path, int i) { printf("Child's index is: %d\n", i); }

char* read_stone(char* path, int i, FILE* fd)
{
    size_t stoneSizeMAX = 512;
    char* stone = malloc(8 * (stoneSizeMAX + 1));
    if (stone == NULL)
        ERR("Stone malloc");

    for (int j = 0; j < 9; j++)
    {
        char* line = malloc(stoneSizeMAX + 1);
        if (line == NULL)
            ERR("Line malloc");
        line = fgets(line, '\n', fd);

        if (line == NULL)
            ERR("getline");

        if (j != 9 && strcmp(line, "\0") != 0)
            strcat(stone, line);

        free(line);
    }

    return stone;
}

void create_children(char* path, int k, sigset_t oldmask)
{
    pid_t pid;

    int fd = TEMP_FAILURE_RETRY(open(path, O_RDONLY));
    if (fd < 0)
        ERR("open");

    FILE* fpointer = fdopen(fd, O_RDONLY);
    if (fpointer == NULL)
    {
        ERR("fdopen");
        return;
    }

    char* stone = "";
    for (int i = 0; i < k; i++)
    {
        int duplicateFD = dup(fd);
        if (duplicateFD < 0)
            ERR("duplicate");

        size_t offset = i * strlen(stone);

        if (-1 == fseek(fpointer, offset, SEEK_SET))
            ERR("fseek");

        stone = read_stone(path, i, fpointer);

        if ((pid = fork()) < 0)  // fork negative
            ERR("fork");

        if (!pid)  // fork 0
        {
            child_work(path, i);
            if (TEMP_FAILURE_RETRY(close(fd)) < 0)
                ERR("close");
            free(stone);
            exit(EXIT_SUCCESS);
        }
    }

    // fork returned pid
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
    free(stone);
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        usage(argc, argv);
    }

    char* path = argv[1];
    int k = atoi(argv[2]);

    if (k <= 0 || k >= 8)
        usage(argc, argv);

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    sethandler(sighandler, SIGUSR1);

    create_children(path, k, oldmask);
    kill(0, SIGUSR1);

    while (1)
    {
        pid_t pid = wait(NULL);
        // echild no child processes
        if (errno == ECHILD && pid < 0)
            break;

        // eintr interrupted function call
        if (errno == EINTR && pid < 0)
        {
            if (last_sig == SIGINT)
                kill(0, SIGINT);
        }
    }

    printf("All workers have gone home.\n");

    return EXIT_SUCCESS;
}
