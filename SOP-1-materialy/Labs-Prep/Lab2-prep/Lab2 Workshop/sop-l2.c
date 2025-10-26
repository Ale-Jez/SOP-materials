#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

sig_atomic_t lastSig;

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

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void usage(int argc, char* argv[])
{
    printf("%s n f \n", argv[0]);
    printf("\tf - file to be processed\n");
    printf("\t0 < n < 10 - number of child processes\n");
    exit(EXIT_FAILURE);
}

void signal_handler(int signal) { lastSig = signal; }

void child_work(char* buff, int size, sigset_t oldmask, char* f, int i)
{
    while (sigsuspend(&oldmask))
    {
        if (lastSig == SIGUSR1)
            break;
    }

    int fd1;
    char name[100] = {0};
    sprintf(name, "%s-%d", f, i + 1);

    if ((fd1 = open(name, O_CREAT | O_TRUNC | O_WRONLY, 0644)) == -1)
        ERR("open:");

    struct timespec time;
    time.tv_nsec = 250000000;
    time.tv_sec = 0;

    for (int j = 0; j < size; j++)
    {
        if (lastSig == SIGINT)
            break;

        bulk_write(fd1, &buff[j], 1);
        while (nanosleep(&time, &time) == -1 && errno == EINTR)
        {
        }
    }

    pid_t pid = getpid();
    printf("%d\n", pid);

    close(fd1);

    return;
}

void create_children(int n, char* f, sigset_t oldmask)
{
    pid_t pid;
    struct stat s;

    if (stat(f, &s) < 0)
        ERR("stat: ");

    int fd;

    if ((fd = open(f, O_RDONLY)) == -1)
        ERR("open");

    off_t fsize = s.st_size;

    char* chunk = malloc((fsize / n + 1) * sizeof(char));

    for (int i = 0; i < n; i++)
    {
        int read;

        if ((read = bulk_read(fd, chunk, fsize / n + 1)) == -1)
            ERR("bulk_read");

        if ((pid = fork()) < 0)
            ERR("create_children:");

        if (!pid)  // child process only
        {
            child_work(chunk, read, oldmask, f, i);
            free(chunk);
            close(fd);
            exit(EXIT_SUCCESS);
        }
    }

    kill(0, SIGUSR1);
    free(chunk);  // indep buffer after forking
    close(fd);
}

int main(int argc, char* argv[])
{
    if (argc != 3)
        usage(argc, argv);

    char* f = argv[1];
    int n = atoi(argv[2]);

    if (n <= 0 || n >= 10)
        usage(argc, argv);

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    sethandler(signal_handler, SIGUSR1);
    sethandler(signal_handler, SIGINT);

    create_children(n, f, oldmask);

    // sigaddset(&mask, SIGINT);
    // sigprocmask(SIG_BLOCK, &mask, &oldmask);

    while (1)
    {
        // SIGINT received here is unhandled

        pid_t pid = wait(NULL);

        if (errno == ECHILD && pid < 0)
        {
            break;
        }

        if (errno == EINTR && pid < 0)
        {
            if (lastSig == SIGINT)
                kill(0, SIGINT);
        }
    }

    return EXIT_SUCCESS;
}
