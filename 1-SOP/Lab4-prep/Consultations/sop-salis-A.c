#include "common.h"
#include <bits/pthreadtypes.h>
#include <bits/types/sigset_t.h>
//#include <csignal>
#include <pthread.h>
//#include <regex>
#include <signal.h>
#include <time.h>


typedef struct argsSignalHandler_t{
    int* doWork;
    pthread_mutex_t* mxDoWork;
    sigset_t* mask;

} argsSignalHandler_t;




void *signal_handling(void *voidArgs)
{
    argsSignalHandler_t *args = voidArgs;
    int signo;
    for (;;)
    {
        if (sigwait(args->mask, &signo))
            ERR("sigwait failed.");
        switch (signo)
        {
            case SIGINT:
                pthread_mutex_lock(args->mxDoWork);
//pointer arithmetic logic
                *(args->doWork) = 0; //dereferencing a pointer to access value the pointer points to
                pthread_mutex_unlock(args->mxDoWork);
                return NULL;
            default:
                printf("unexpected signal %d\n", signo);
                exit(1);
        }
    }
    return NULL;
}


int main(int argc, char* argv[])
{
    srand(time(NULL));

    int do_work = 1;
    pthread_mutex_t do_work_mutex = PTHREAD_MUTEX_INITIALIZER;


    if (argc != 3)
        usage(argc, argv);

    int N;
    N = atoi(argv[1]);
    if (N <= 0 || N > 20)
        usage(argc, argv);

    int Q;
    Q = atoi(argv[2]);
    if (Q <= 0 || Q > 10)
        usage(argc, argv);




    //create a signal thread - it will handle all the signals that are passed
    pthread_t sigID;
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    argsSignalHandler_t sigArgs;
    sigArgs.doWork = &do_work;
    sigArgs.mask = &mask;
    sigArgs.mxDoWork = & do_work_mutex;

    pthread_create(&sigID, NULL, signal_handling, &sigArgs);


    while (1) {
        pthread_mutex_lock(&do_work_mutex);
        if (do_work == 0){
            
            pthread_mutex_unlock(&do_work_mutex);
            break;
        }
    }

    pthread_join(sigID,NULL);
}
