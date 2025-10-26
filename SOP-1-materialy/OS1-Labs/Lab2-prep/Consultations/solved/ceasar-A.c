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

void child_work(){
    printf("CHILD: [%d]\n", getpid());

}





void create_children(int k, char* p)
{
    // Pamiętaj aby poprawnie obsłużyć błąd EINTR, chodzi o stosowanie TEMP_FAILURE_RETRY

    pid_t pid;




    for (int i = 0; i < k; i++)
    {
         if ((pid = fork()) < 0)
            ERR("fork");

        // Child process only
        if (!pid)
        {
            child_work();
           
            exit(EXIT_SUCCESS);
        }

        // Parent process continues
        // child process exited in if condition
    }

}




int main(int argc, char* argv[])
{
    if (argc != 3)
        usage(argc, argv);

    char* p = argv[1];
    int k = atoi(argv[2]);

    if (k <= 0 || k >= 8)
        usage(argc, argv);

    printf("PARENT: [%d]\n", getpid());

    create_children(k, p);


    while (1)
    {
        pid_t pid = wait(NULL);

        if (errno == ECHILD && pid < 0)
            break;
    }



    return EXIT_SUCCESS;
}
