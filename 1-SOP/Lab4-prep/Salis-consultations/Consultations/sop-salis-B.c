#include "common.h"
//#include <algorithm>
#include <bits/pthreadtypes.h>
#include <bits/types/sigset_t.h>
//#include <csignal>
//#include <cstdio>
#include <pthread.h>
//#include <regex>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


typedef struct argsSignalHandler_t{
    int* doWork;
    pthread_mutex_t* mxDoWork;
    sigset_t* mask;

} argsSignalHandler_t;




typedef struct field_t{
    int noSacks;
    int noSalted;
    pthread_mutex_t mxNoSacks;

}field_t;


//for individual porter
typedef struct argsPorter_t{
    unsigned int seed;
    field_t* fields;
    int noFields;
    int*  doWork;
    pthread_mutex_t* mxDoWork;

}argsPorter_t;


void* porter_routine(void *voidArgs)
{
    argsPorter_t* args = voidArgs;
    while(1)
    {
        pthread_mutex_lock(args->mxDoWork);
//dereferencing a pointer
        if (*(args->doWork) == 0){
            pthread_mutex_unlock(args->mxDoWork);
            break;
        }
        pthread_mutex_unlock(args->mxDoWork);

//ensure thread safe randomness
        rand_r(&args->seed);
        int field = args->seed % args->noFields;
        
        pthread_mutex_lock(&(args->fields[field].mxNoSacks));
        args->fields[field].noSacks += 5;
        printf("Current salt bags in field[%d]: %d\n", field, args->fields[field].noSacks);
        pthread_mutex_unlock(&(args->fields[field].mxNoSacks));

//simulate work
        msleep(5 + field);

    }
    return NULL;
}



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
//signal handling
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    argsSignalHandler_t sigArgs;
    sigArgs.doWork = &do_work;
    sigArgs.mask = &mask;
    sigArgs.mxDoWork = & do_work_mutex;

    pthread_create(&sigID, NULL, signal_handling, &sigArgs);


//create Carthago fields
    field_t* fields = calloc(N, sizeof(field_t));
    if (fields == NULL)
        ERR("calloc");

    for (int i = 0; i < N; i++)
    {
        pthread_mutex_init(&fields[i].mxNoSacks, NULL);
    }


//create porter threads
    pthread_t* porterID;
    porterID = calloc(Q, sizeof(pthread_t));
    if (porterID == NULL)
        ERR("calloc");
    argsPorter_t* porterArgs = calloc(Q, sizeof(argsPorter_t));
    if (porterArgs == NULL)
        ERR("calloc");


    for (int i = 0; i < Q; i++)
    {
        porterArgs[i].doWork = &do_work;
        porterArgs[i].fields = fields;
        porterArgs[i].mxDoWork = &do_work_mutex;
        porterArgs[i].noFields = N;
        porterArgs[i].seed = rand();        
        
        pthread_create(&porterID[i], NULL, porter_routine, &porterArgs[i]);
    }


//cleanup
    for (int i = 0; i < Q; i++)
    {
        pthread_join(porterID[i], NULL);
    }

    



    for (int i = 0; i < N; i++)
    {
        pthread_mutex_destroy(&fields[i].mxNoSacks);
    }


    free(porterID);
    free(porterArgs);
    free(fields);

    pthread_join(sigID,NULL);
}
