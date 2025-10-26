
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
int probability = -1;
int total_issues = 0;
int sigusr1_count = 0;
struct Map* tab;

struct Map
{
    int pid;
    int issues;
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

void set_info_handler(void (*f)(int, siginfo_t*, void*), int sig)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = f;
    act.sa_flags = SA_SIGINFO;
    if (sigaction(sig, &act, NULL) < 0)
        ERR("sigaction");
}

void sig1_handler(int sig, siginfo_t* siginfo, void* u)
{
    sigusr1_count++;
    printf("Teacher has accepted solution of student [%d]\n", siginfo->si_pid);
    if (kill(siginfo->si_pid, SIGUSR2) < 0)
        ERR("parent sigusr2");
}

void sig_handler(int sig) { last_signal = sig; }

void child_print(int prob)
{
    probability = prob;
    printf("%d. %d\n", getpid(), probability);
}

void sigchild_handler(int sig)
{
    pid_t pid;
    int stat;
    static int index = 0;
    while (1)
    {
        pid = waitpid(0, &stat, WNOHANG);
        if (pid > 0)
        {
            tab[index].pid = pid;
            tab[index].issues = WEXITSTATUS(stat);
            index++;
            total_issues += WEXITSTATUS(stat);
            printf("%d\n", pid);
        }
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

void child_work(int prob, int parts, int time, int idx)
{
    last_signal = 0;
    srand(getpid());
    set_handler(SIG_DFL, SIGUSR1);
    set_handler(sig_handler, SIGUSR2);
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    probability = prob;
    printf("Student[%d,%d] has started doing task!\n", idx, getpid());
    int counter = 0;
    for (int i = 0; i < parts; i++)
    {
        int chance = 1 + rand() % prob;
        struct timespec tp = {0, time * 100 * 1000000};
        printf("Student[%d,%d] is starting doing part %d of %d!\n", idx, getpid(), i + 1, parts);
        if (chance % prob == 0)
        {
            tp.tv_nsec += 50 * 1000000;
            printf("Student[%d,%d] has issue (%d) doing task!\n", idx, getpid(), ++counter);
        }
        while (nanosleep(&tp, &tp) == -1 && errno != EINTR)
            ;
        // nanosleep(&tp,NULL);

        printf("Student[%d,%d] has finished part %d of %d!\n", idx, getpid(), i + 1, parts);

        if ((kill(getppid(), SIGUSR1)) < 0)
            ERR("kill sigusr1");
        while (last_signal != SIGUSR2)
            sigsuspend(&oldmask);
    }
    exit(counter);
}

void create_child(int n, int* prob, int parts, int time)
{
    pid_t pid;
    for (int i = 0; i < n; i++)
    {
        if ((pid = fork()) < 0)
            ERR("fork");
        if (!pid)
        {
            child_work(prob[i], parts, time, i + 1);
            free(prob);
            free(tab);
            exit(0);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc < 4)
        usage(argv[0]);

    int parts = atoi(argv[1]);
    int time = atoi(argv[2]);
    int no_students = argc - 3;
    int* prob = (int*)malloc(no_students * sizeof(int));
    for (int i = 3; i < argc; i++)
    {
        prob[i - 3] = atoi(argv[i]);
    }

    set_info_handler(sig1_handler, SIGUSR1);
    set_handler(sigchild_handler, SIGCHLD);
    create_child(no_students, prob, parts, time);

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    tab = (struct Map*)malloc(sizeof(struct Map) * no_students);

    while (sigusr1_count < no_students * parts)
    {
        sigsuspend(&oldmask);
    }

    sigprocmask(SIG_UNBLOCK, &mask, &oldmask);

    while (wait(NULL) > 0)
        ;

    printf("No. | Student ID | Issue count\n");

    for (int i = 0; i < no_students; i++)
    {
        printf("%3d | %10d | %d\n", i + 1, tab[i].pid, tab[i].issues);
    }

    printf("Total issues: %d\n", total_issues);
    printf("parent be terminating\n");
    free(tab);
    free(prob);
    return EXIT_SUCCESS;
}
