
#define _GNU_SOURCE
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
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t last_signal = 0;
int count = -1;
volatile sig_atomic_t terminate_parent = 0;

struct Map
{
    int index;
    int pid;
};

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s \n", name);
    exit(EXIT_FAILURE);
}

/*
-p parts
-t * 100ms  time to finish =>
Task takes : p*t*100ms to finish

p, t , list of nums

-list of nums => how many addtionnal 50 ms

-sigusr1 to raise hand => parent send sigusr2 to cont
-after all tasks exit with status equal to no of issues during the task

-terminate at 100
*/

/// Handlers

void set_handler(void (*f)(int), int sig)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) < 0)
        ERR("sigaction");
}

void sig_handler(int sig) { last_signal = sig; }

void sigint_parent(int sig)
{
    // if(kill(0,SIGINT)<0) ERR("sigint");
    terminate_parent = 1;
}

void sigchld_handler(int sig)
{
    pid_t pid;
    while (1)
    {
        pid = waitpid(0, NULL, WNOHANG);
        if (pid == 0)
            return;
        if (pid < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno == ECHILD)
                return;
            ERR("waitpid");
        }
    }
}

void child_work(sigset_t oldmask)
{
    // printf("%d %d\n",count,getpid());
    srand(getpid());
    set_handler(sig_handler, SIGINT);
    int counter = 0;
    last_signal = 0;
    while (1)
    {
        if (last_signal == SIGINT)
        {
            int out;
            char buff[100];
            snprintf(buff, sizeof(buff), "%d.txt", getpid());

            if ((out = open(buff, O_CREAT | O_TRUNC | O_WRONLY, 0644)) == -1)
                ERR("open");
            snprintf(buff, sizeof(buff), "%d\n", counter);
            write(out, buff, sizeof(int));
            if (close(out))
                ERR("close");
            exit(0);
        }

        while (last_signal != SIGUSR1)
            sigsuspend(&oldmask);

        int time = 100 + rand() % 100;
        struct timespec tp = {0, time * 1000000};
        nanosleep(&tp, NULL);
        printf("%d: %d\n", getpid(), ++counter);
        if (kill(getppid(), SIGUSR1) < 0)
            ERR("child sigusr2");
    }
}

void create_child(int n, sigset_t oldmask, struct Map* map)
{
    pid_t pid;
    for (int i = 0; i < n; i++)
    {
        if ((pid = fork()) < 0)
            ERR("fork");
        if (!pid)
        {
            free(map);
            child_work(oldmask);
            exit(0);
        }
        else
        {
            map[i].pid = pid;
            map[i].index = count + i + 1;
        }
    }
}

void parent_work(sigset_t oldmask, struct Map* children, int n)
{
    int curr = 0;
    while (!terminate_parent)
    {
        if (kill(children[curr].pid, SIGUSR1) < 0)
            ERR("parent_work: sigusr2");
        while (last_signal != SIGUSR1)
            sigsuspend(&oldmask);
        // stop the work;
        if (kill(children[curr].pid, SIGUSR2) < 0)
            ERR("parent_work: sigusr2");
        curr = (curr + 1) % n;
        last_signal = 0;
    }
}

/*
*child waits for sigusr1
1. parent sends sigusr1
2. child does its work & send sigusr1;
3.
*/

int main(int argc, char** argv)
{
    if (argc != 2)
        usage(argv[0]);
    int n = atoi(argv[1]);

    set_handler(sigchld_handler, SIGCHLD);
    set_handler(sig_handler, SIGUSR1);
    set_handler(sig_handler, SIGUSR2);
    set_handler(sigint_parent, SIGINT);

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    struct Map* children = (struct Map*)malloc(n * sizeof(struct Map));


    create_child(n, oldmask, children);
    if (count == -1)
        parent_work(oldmask, children, n);

    int x;
    while ((x = wait(NULL)) > 0)
    {
        if (errno == ECHILD)
            break;
        printf("%d\n", x);
    }
    printf("parent terminates\n");

    /*
    if(count==-1)
    {
        for(int i=0;i<n;i++)
        {
            printf("%d bozo %d\n",children[i].pid,children[i].index);
        }
    }
    */

    sigprocmask(SIG_UNBLOCK, &mask, &oldmask);
    free(children);
    return EXIT_SUCCESS;
}
