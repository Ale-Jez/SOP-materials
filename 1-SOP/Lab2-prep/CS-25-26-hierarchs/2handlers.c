#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

// based on:
// https://github.com/mixasolop/ops_lab2_prep
// https://sop.mini.pw.edu.pl/en/sop1/lab/l2/

#define DELAY_MS 100
volatile sig_atomic_t last_sig;
volatile sig_atomic_t last_sig1;
volatile sig_atomic_t last_sig2;
void sig_handler(int sig) { last_sig = sig; }
void sig_handler_usr1(int sig) { last_sig1 = sig; }
void sig_handler_usr2(int sig) { last_sig2 = sig; }

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

void ms_sleep(unsigned int milli)
{
    time_t sec = (int)(milli / 1000);
    milli = milli - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milli * 1000000L;
    if (TEMP_FAILURE_RETRY(nanosleep(&ts, &ts)))
        ERR("nanosleep");
}

void usage(int argc, char* argv[])
{
    printf("%s k name_1 [name_2 [...]]\n", argv[0]);
    printf("\t1 <= k -- number of pages to sign\n");
    printf("\tname_i -- name of i-th signatory\n");
    exit(EXIT_FAILURE);
}

//
void child_work(int totalNum, pid_t* pids, char* name, int myNum)
{
    printf("[%d] My name is %s.\n", getpid(), name);

    sethandler(sig_handler_usr1, SIGUSR1);
    sethandler(sig_handler_usr2, SIGUSR2);
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    //
    while (sigsuspend(&oldmask))
    {
        if (last_sig1 == SIGUSR1)
        {
            printf("[%d] I, %s, sign the declaration\n", getpid(), name);
            // printf("[%d] I, %s, my num %d, total num %d\n", getpid(), name, myNum, totalNum);
            ms_sleep(DELAY_MS);
            if (totalNum >= 2)
            {
                if (myNum - 1 > 0)
                {
                    if (kill(pids[myNum - 1], SIGUSR1) == -1)
                        ERR("kill");
                    return;
                }
            }
            break;
        }
        if (last_sig2 == SIGUSR2)
        {
            printf("[%d] Done!\n", getpid());
            if (totalNum >= 2)
            {
                if (myNum - 1 > 0)
                {
                    if (kill(pids[myNum - 1], SIGUSR2) == -1)
                        ERR("kill");
                    return;
                }
                if (myNum - 1 == 0)
                {
                    if (kill(getppid(), SIGUSR2) == -1)
                        ERR("kill");
                    return;
                }
            }
            break;
        }
    }
}

//
void parent_work(int num, pid_t* pids, int k)
{
    ms_sleep(200);

    sethandler(sig_handler_usr1, SIGUSR1);
    sethandler(sig_handler_usr2, SIGUSR2);
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    // printf("[%d] Parents sends SIGUSR1 to child no %d its' pid is [%d]\n", getpid(), num, pids[num - 1]);
    for (int i = 0; i < k; i++)
    {
        if (kill(pids[num - 1], SIGUSR1) == -1)
            ERR("kill");
        ms_sleep(DELAY_MS);
    }
    if (kill(pids[num - 1], SIGUSR2) == -1)
        ERR("kill");

    while (sigsuspend(&oldmask))
    {
        if (last_sig2 == SIGUSR2)
        {
            printf("[%d] got SIGUSR2\n", getpid());
            exit(EXIT_SUCCESS);
            return;
        }
    }
}

//

void deploy_hierarchs(int num, pid_t* pids, char* names[], int k)
{
    printf("There are %d hierarchs\n", num);

    for (int i = 0; i < num; i++)
    {
        pid_t child_pid = fork();
        pids[i] = child_pid;
        if (child_pid < 0)
            ERR("fork");
        else if (child_pid == 0)
        {
            child_work(num, pids, names[i], i);
            exit(EXIT_SUCCESS);
        }
    }
    parent_work(num, pids, k);
    //
}

//

int main(int argc, char* argv[])
{
    if (argc <= 2)
        usage(argc, argv);

    int k = atoi(argv[1]);
    if (k <= 0 || k >= 26)
        usage(argc, argv);

    int num = argc - 2;
    char* hierarchs[num];
    int numCopy = num;
    while (numCopy > 0)
    {
        hierarchs[num - numCopy] = argv[num - numCopy + 2];
        numCopy--;
    }

    pid_t children_pid[num];
    deploy_hierarchs(num, children_pid, hierarchs, k);

    while (wait(NULL) > 0)
    {
    }
    printf("Done\n");

    return EXIT_SUCCESS;
}
