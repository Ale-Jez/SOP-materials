#define _GNU_SOURCE
#include <bits/types/sigset_t.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (kill(0, SIGKILL), perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define FILE_MAX_SIZE 512

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

volatile sig_atomic_t sigusr1 = 0;
volatile sig_atomic_t sigusr2 = 0;
volatile sig_atomic_t sigint = 0;

void usr1_handler() { sigusr1 = 1; }
void usr2_handler() { sigusr2 = 1; }
void int_handler() { sigint = 1; }

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
    printf("%s p h\n", argv[0]);
    printf("\tp - path to directory describing the structure of the Austro-Hungarian office in Prague.\n");
    printf("\th - Name of the highest administrator.\n");
    exit(EXIT_FAILURE);
}

//
void document_logic(char* name, const sigset_t* oldmask, int is_root)
{
    srand(getpid());

    while (sigsuspend(oldmask))
        if (sigint)
        {
            printf("%s ending the dya.\n", name);
            if (children[0] > 0)
                kill(children[0], SIGINT);
            if (children[1] > 0)
                kill(children[1], SIGINT);
        }

    if (sigusr2 == 1)
    {
        sigusr2 = 0;
        if (is_root == 1)
            printf("%s received a document. Ignoring.\n", name);
        else
        {
            if ((rand() % 3) == 0)
                printf("%s received a document. Sending to the archive.\n", name);
            else
            {
                printf("%s received a document. Passing on to the superindtendent.\n", name);
                pid_t parent_pid = getppid();
                if (parent_pid != 0)
                    kill(parent_pid, SIGUSR2);
            }
        }
    }
}
//

void clerk_work(char* name, const sigset_t* oldmask, int is_root)
{
    printf("My name is %s and my PID is %d\n", name, getpid());

    const int fd = open(name, O_RDONLY);
    if (fd == -1)
        ERR("open");

    char buf[FILE_MAX_SIZE - 1] = {0};
    if (bulk_read(fd, buf, FILE_MAX_SIZE) <= 0)
        ERR("bulk_read");
    if (close(fd) == -1)
        ERR("close");

    char* child_name = strtok(buf, "\n");
    while (child_name)
    {
        if (strcmp(child_name, "-") == 0)
        {
            child_name = strtok(NULL, "\n");
            continue;
        }

        printf("%s inspecting %s\n", name, child_name);

        pid_t pid_child = fork();
        if (pid_child < 0)
            ERR("fork");
        else if (pid_child == 0)  // Child process
        {
            clerk_work(child_name, oldmask, 0);
            exit(EXIT_SUCCESS);  // without it we get fork-bomb
        }
        else  // Parent process
            child_name = strtok(NULL, "\n");
        //
    }
    printf("%s has inspected all subordinates\n", name);

    document_logic(name, oldmask, is_root);

    // waiting for all the children to die out
    while (wait(NULL) > 0)
    {
    }
    printf("%s is leaving the office\n", name);
}

//

int main(int argc, char* argv[])
{
    if (argc != 3)
        usage(argc, argv);
    //
    if (chdir(argv[1]))
        ERR("chdir");

    //
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    sethandler(usr1_handler, SIGUSR1);
    sethandler(usr2_handler, SIGUSR2);
    sethandler(int_handler, SIGINT);

    printf("[%d] Waiting for SIGUSR1\n", getpid());

    while (sigsuspend(&oldmask))
    {
        if (sigusr1 == 1)
        {
            printf("Got SIGUSR1\n");
            break;
        }
    }

    //
    clerk_work(argv[2], &oldmask, 1);
}
