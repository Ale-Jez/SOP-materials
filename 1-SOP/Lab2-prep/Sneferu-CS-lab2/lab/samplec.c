#include <csignal>
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
#define ERR(source)                                                            \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),             \
   kill(0, SIGKILL), exit(EXIT_FAILURE))



volatile sig_atomic_t sigusr1_rec = 0;
volatile sig_atomic_t sigint_rec = 0;

void sigchld_handler(int sig)
{
    pid_t pid;
    for (;;)
    {
        pid = waitpid(0, NULL, WNOHANG);
        if (pid == 0)
            return;
        if (pid <= 0)
        {
            if (errno == ECHILD)
                return;
            ERR("waitpid");
        }
    }
}

void sighandler(int sig)
{
    if (sig == SIGUSR1)
        sigusr1_rec = 1;
    if (sig == SIGINT)
        sigint_rec = 1;
}

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void block_signal(int sig)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, sig);
    sigprocmask(SIG_BLOCK, &mask, NULL);
}


//z maina -------------------------------
//     sigset_t blockUSR1, unblockSIGS;
//     sigfillset(&blockUSR1);
//     sigaddset(&blockUSR1, SIGUSR1);
//     sigprocmask(SIG_BLOCK, &blockUSR1, &unblockSIGS); 
    
//     sethandler(sigchld_handler, SIGCHLD);
//     sethandler(sighandler, SIGUSR1);
//     sethandler(sighandler, SIGINT);