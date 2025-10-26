#define _GNU_SOURCE
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

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

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
    printf("\tp - path to file to be encrypted\n");
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

void sighandler(int signo) { last_sig = signo;}

void child_work(char* chunk, int readbytes, sigset_t oldmask, char* p, int i){
    printf("CHILD: [%d]\n", getpid());

    while (last_sig != SIGUSR1)
    {
        sigsuspend(&oldmask);
    }

    char name[100];
    sprintf(name, "%s-%d", p, i);

//creating a file
//O_CREAT | O_TRUNC | O_APPEND | O_WRONLY, 0644
    int fd;
    if ((fd = TEMP_FAILURE_RETRY(open(name, O_CREAT | O_TRUNC | O_APPEND | O_WRONLY, 0644))) < 0)
        ERR("open");

    for (int j = 0; j < readbytes; j++)
    {
        // Dodaj poprawną obsługę sygnału SIGINT
        if (last_sig == SIGINT)
        {
            // Proces dziecka po otrzymaniu SIGINT [...]
            // wysłać SIGINT do wszystkich innych procesów w grupie
            kill(0, SIGINT);  // Parent ma zablokowany sigint więc nie umrze
            printf("I am the Child: %d I kill myself\n", getpid());
            break;
        }

        // szyfrowanie
        if (chunk[j] == 'z')
            chunk[j] = 'c';
        else if (chunk[j] == 'y')
            chunk[j] = 'b';
        else if (chunk[j] == 'x')
            chunk[j] = 'a';
        else
            chunk[j] += 3;

        bulk_write(fd, &chunk[j], 1);

        // Między zamianami kolejnych znaków proces śpi 0.1 sekundy.
        // Poprawnie obsłuż przypadek przerwania snu sygnałem
        struct timespec t;
        t.tv_nsec = 100000000;
        t.tv_sec = 0;

        while (nanosleep(&t, &t) == -1 && errno == EINTR)
        {}
    }

    // chunk is freed after child_work finishes, in create_children function
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");




}





void create_children(int k, char* p, sigset_t oldmask)
{
    // Pamiętaj aby poprawnie obsłużyć błąd EINTR, chodzi o stosowanie TEMP_FAILURE_RETRY

    pid_t pid;

    int fd;
    if ((fd = TEMP_FAILURE_RETRY(open(p, O_RDONLY))) < 0)
        ERR("open");

    // Zawartość plików dzielona jest po równo
    struct stat s;
    if (stat(p, &s) < 0)
        ERR("stat");
    
    off_t fsize = s.st_size;
    char* chunk = malloc((fsize / k + 1) * sizeof(char));
    if (chunk == NULL)
        ERR("malloc");




    for (int i = 0; i < k; i++)
    {
        int bytes_read;

        if ((bytes_read = bulk_read(fd, chunk, (fsize / k + 1) * sizeof(char))) == -1)
            ERR("bulk_read");

        if ((pid = fork()) < 0)
            ERR("fork");

        // Child process only
        if (!pid)
        {
            child_work(chunk, bytes_read, oldmask, p, i);
            free(chunk);
            exit(EXIT_SUCCESS);
        }

        // Parent process continues
        // child process exited in if condition
    }
    free(chunk);

    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");

}




int main(int argc, char* argv[])
{
    if (argc != 3)
        usage(argc, argv);

    char* p = argv[1];
    int k = atoi(argv[2]);

    if (k <= 0 || k >= 8)
        usage(argc, argv);

//oldmask stores the previous signal mask (can be NULL)
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    sethandler(sighandler, SIGUSR1);
    sethandler(sighandler, SIGINT);



    printf("PARENT: [%d]\n", getpid());

    create_children(k, p, oldmask);
    kill (0, SIGUSR1);


    while (1)
    {
        pid_t pid = wait(NULL);

        //ECHILD confirms there are no child proccesses left
        if (errno == ECHILD && pid < 0)
            break;


        // Dodaj poprawną obsługę sygnału SIGINT
        // dostaliśmy siginta, wait ustawi errno na EINTR
        if (errno == EINTR && pid < 0){
        //EINTR Confirms that the interruption was due to a signal.
            if (last_sig == SIGINT){
                kill(0, SIGINT);  // rodzic się nie zabija sigintem bo ma inny hanlder niż dzieci więc jest git
                printf("\nI am the Parent %d I kill myself\n", getpid());
            }
        }
    }



    return EXIT_SUCCESS;
}
