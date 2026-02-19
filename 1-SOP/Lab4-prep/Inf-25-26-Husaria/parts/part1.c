#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
/*
based and inspired from:
https://pages.mini.pw.edu.pl/~hermant/en/courses/operating-systems-1/l4-2025/
https://github.com/Ale-Jez/SOP-materials
https://github.com/michau-s/OPS1-L4-Consultations
*/



typedef struct {
    sem_t gorge_sem;   // Restricts access to the gorge (Stage 1)
    // Future stages will add: enemy_hp, mutexes, barriers, etc.
} SharedData_t;

typedef struct {
    int id;
    SharedData_t *shared;
} ThreadArgs_t;


void ms_sleep(unsigned int milli)
{
    struct timespec ts = {milli / 1000, (milli % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

void usage(int argc, char* argv[])
{
    printf("%s N M\n", argv[0]);
    printf("\t10 <= N <= 20 - number of banner threads\n");
    printf("\t2 <= M <= 8 - number of artillery threads\n");
    exit(EXIT_FAILURE);
}


void* cavalry_thread(void* arg) {
    ThreadArgs_t* args = (ThreadArgs_t*)arg;
    SharedData_t* shared = args->shared;

    if (sem_wait(&shared->gorge_sem) == -1) ERR("sem_wait");

    int duration = 80 + (rand() % 41); 
    ms_sleep(duration);

    if (sem_post(&shared->gorge_sem) == -1) ERR("sem_post");

    printf("CAVALRY %d: IN POSITION\n", args->id);

    return NULL;
}


int main(int argc, char* argv[])
{
    if (argc != 3) 
        usage(argc, argv);

    int N = atoi(argv[1]);
    int M = atoi(argv[2]);

    if (N < 10 || N > 20 || M < 2 || M > 8) usage(argc, argv);

    srand(time(NULL));

    SharedData_t shared;
    if (sem_init(&shared.gorge_sem, 0, 3) != 0) ERR("sem_init");

    pthread_t* banners = calloc(sizeof(pthread_t), N);
    ThreadArgs_t* banner_args = calloc(sizeof(ThreadArgs_t), N);
    
    if (!banners || !banner_args) ERR("calloc");

    for (int i = 0; i < N; i++) {
        banner_args[i].id = i;
        banner_args[i].shared = &shared;
        if (pthread_create(&banners[i], NULL, cavalry_thread, &banner_args[i]) != 0) 
            ERR("pthread_create");
    }

    for (int i = 0; i < N; i++) {
        pthread_join(banners[i], NULL);
    }

    free(banners);
    free(banner_args);
    sem_destroy(&shared.gorge_sem);

    return EXIT_SUCCESS;
}

