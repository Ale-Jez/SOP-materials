#include "common.h"
//#include <algorithm>
#include <bits/pthreadtypes.h>
#include <bits/types/siginfo_t.h>
#include <bits/types/sigset_t.h>
//#include <csignal>
//#include <cstdio>
//#include <cerrno>
#include <pthread.h>
//#include <regex>
#include <semaphore.h>
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
//conditional variable, true if mxNoSacks > 0
    pthread_cond_t condNoSacks;

}field_t;


//for individual porter
typedef struct argsPorter_t{
    unsigned int seed;
    field_t* fields;
    int noFields;
    int*  doWork;
    pthread_mutex_t* mxDoWork;
    sem_t* semaphore;

}argsPorter_t;


//for individual laborer
typedef struct argsLaborer_t{
    field_t* field;
    int ID;
    int*  doWork;
    pthread_mutex_t* mxDoWork;
    
}argsLaborer_t;



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

        if (sem_wait(args->semaphore))
            ERR("sem_wait");
//simulate work
        msleep(5);
        if (sem_post(args->semaphore))
            ERR("sem_post");
        


//ensure thread safe randomness
        rand_r(&args->seed);
        int field = args->seed % args->noFields;
        
        pthread_mutex_lock(&(args->fields[field].mxNoSacks));
        args->fields[field].noSacks += 5;

        pthread_mutex_unlock(&(args->fields[field].mxNoSacks));

//signal the conditional variable
        pthread_cond_signal(&(args->fields[field].condNoSacks));



//simulate work
        msleep(5 + field);
        

    }
    return NULL;
}


void* laborer_routine(void* voidArgs)
{
    argsLaborer_t* args = voidArgs;

    while(1)
    {
        pthread_mutex_lock(args->mxDoWork);
//dereferencing a pointer
        if (*(args->doWork) == 0){
            pthread_mutex_unlock(args->mxDoWork);
            break;
        }
        pthread_mutex_unlock(args->mxDoWork);
    
        pthread_mutex_lock(&(args->field->mxNoSacks));

        
//cond variables are in cost checkups thats why we use while NOT if
        while (args->field->noSacks <= 0) {
//using a function from common.h
            struct timespec t = get_cond_wait_time(100);
            int ret = pthread_cond_timedwait(&(args->field->condNoSacks), &(args->field->mxNoSacks), &t);

            pthread_mutex_lock(args->mxDoWork);
            if (*(args->doWork) == 0){
                pthread_mutex_unlock(args->mxDoWork);
                break;
            }
            pthread_mutex_unlock(args->mxDoWork);

            if (ret == ETIMEDOUT){
                printf("SERVVS %d: EXPECTO SALEM\n", args->ID);
            }
        }


//salting the fields
        args->field->noSacks--;
        pthread_mutex_unlock(&(args->field->mxNoSacks));

        msleep(15);
        args->field->noSalted++;
        printf("SERVVS %d: ADIPISCOR SALEM %d ADHVC IVGERVM\n", args->ID, args->field->noSalted);
        
        pthread_mutex_unlock(&(args->field->mxNoSacks));
        if (args->field->noSalted >= 50){
            printf("SERVVS %d: LABOR MEVS CONFLICTO\n", args->ID);
            return NULL;
        }
    
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
    
    argsLaborer_t* laborerArgs;
    laborerArgs = calloc(N, sizeof(argsLaborer_t));
    if (laborerArgs == NULL)
        ERR("calloc");

    pthread_t* laborerID;
    laborerID = calloc(N, sizeof(laborerID));
    if (laborerID == NULL)
        ERR("calloc");

    for (int i = 0; i < N; i++)
    {
        pthread_mutex_init(&fields[i].mxNoSacks, NULL);
        pthread_cond_init(&fields[i].condNoSacks, NULL);

        laborerArgs[i].field = &fields[i];
        laborerArgs[i].ID = i;
        laborerArgs[i].mxDoWork = &do_work_mutex;
        laborerArgs[i].doWork = &do_work;

        pthread_create(&laborerID[i], NULL, laborer_routine, &laborerArgs[i]);
    }




//create porter threads
    pthread_t* porterID;
    porterID = calloc(Q, sizeof(pthread_t));
    if (porterID == NULL)
        ERR("calloc");
    argsPorter_t* porterArgs = calloc(Q, sizeof(argsPorter_t));
    if (porterArgs == NULL)
        ERR("calloc");

//making a semaphore
    sem_t semaphore;
    sem_init(&semaphore, 0, 3);


    for (int i = 0; i < Q; i++)
    {
        porterArgs[i].doWork = &do_work;
        porterArgs[i].fields = fields;
        porterArgs[i].mxDoWork = &do_work_mutex;
        porterArgs[i].noFields = N;
        porterArgs[i].seed = rand();                
        porterArgs[i].semaphore = &semaphore;                

        
        pthread_create(&porterID[i], NULL, porter_routine, &porterArgs[i]);
    }


//cleanup  
    for (int i = 0; i < N; i++)
    {
        pthread_cond_destroy(&fields[i].condNoSacks);
        pthread_mutex_destroy(&fields[i].mxNoSacks);
        pthread_join(laborerID[i], NULL);
    }

//killing
    kill(0, SIGINT);

//if all fields have been salted
    if (do_work == 1){
        printf("SCIPIO: VENI, VIDI, VICI, CONSPERSI\n");
    }


    for (int i = 0; i < Q; i++)
    {
        pthread_join(porterID[i], NULL);
    }

    free(porterID);
    free(porterArgs);
    free(fields);

    free(laborerID);
    free(laborerArgs);

    sem_destroy(&semaphore);

    pthread_join(sigID,NULL);
}
