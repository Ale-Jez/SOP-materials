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

typedef struct {
    sem_t gorge_sem;
    int enemy_hp;
    pthread_mutex_t hp_mutex;       // Protects enemy_hp
    pthread_barrier_t art_barrier;  // Synchronizes artillery volleys
    pthread_cond_t charge_cv;       // Signals cavalry to charge
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

    // --- Wait for Artillery ---
    if (pthread_mutex_lock(&shared->hp_mutex) != 0) ERR("pthread_mutex_lock");
    while (shared->enemy_hp >= 50) {
        pthread_cond_wait(&shared->charge_cv, &shared->hp_mutex);
    }
    if (pthread_mutex_unlock(&shared->hp_mutex) != 0) ERR("pthread_mutex_unlock");

    printf("CAVALRY %d: READY TO CHARGE\n", args->id);
    return NULL;
}


void* artillery_thread(void* arg) {
    ThreadArgs_t* args = (ThreadArgs_t*)arg;
    SharedData_t* shared = args->shared;

    while (1)
    {
    ms_sleep(400);
        int damage = 1 + (rand() % 6);
        
        if (pthread_mutex_lock(&shared->hp_mutex) != 0) ERR("pthread_mutex_lock");
        shared->enemy_hp -= damage;
        if (pthread_mutex_unlock(&shared->hp_mutex) != 0) ERR("pthread_mutex_unlock");

        //--- 2. Synchronize Volley ---
        int res = pthread_barrier_wait(&shared->art_barrier);
        if (res == PTHREAD_BARRIER_SERIAL_THREAD) {
            printf("ARTILLERY: ENEMY HP %d\n", shared->enemy_hp);
        }

        if (pthread_mutex_lock(&shared->hp_mutex) != 0) ERR("pthread_mutex_lock");
        if (shared->enemy_hp < 50) {
            // Wake up ALL waiting cavalry
            pthread_cond_broadcast(&shared->charge_cv);
            if (pthread_mutex_unlock(&shared->hp_mutex) != 0) ERR("pthread_mutex_unlock");
            break; // Stop firing
        }
        if (pthread_mutex_unlock(&shared->hp_mutex) != 0) ERR("pthread_mutex_unlock");

    }
        
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
    shared.enemy_hp = 100;
    if (pthread_mutex_init(&shared.hp_mutex, NULL) != 0) ERR("pthread_mutex_init");
    if (pthread_cond_init(&shared.charge_cv, NULL) != 0) ERR("pthread_cond_init");
    if (pthread_barrier_init(&shared.art_barrier, NULL, M) != 0) ERR("pthread_barrier_init");

    pthread_t* banners = calloc(sizeof(pthread_t), N);
    ThreadArgs_t* banner_args = calloc(sizeof(ThreadArgs_t), N);
    if (!banners || !banner_args) ERR("calloc");

    for (int i = 0; i < N; i++) {
        banner_args[i].id = i;
        banner_args[i].shared = &shared;
        if (pthread_create(&banners[i], NULL, cavalry_thread, &banner_args[i]) != 0) 
            ERR("pthread_create");
    }


    pthread_t* artillery = calloc(sizeof(pthread_t), M);
    ThreadArgs_t* artillery_args = calloc(sizeof(ThreadArgs_t), M);
    if (!artillery || !artillery_args) ERR("calloc");
    for (int i = 0; i < M; i++) {
        artillery_args[i].id = i;
        artillery_args[i].shared = &shared;
        if (pthread_create(&artillery[i], NULL, artillery_thread, &artillery_args[i]) != 0) 
            ERR("pthread_create");
    }


    for (int i = 0; i < N; i++) {
        pthread_join(banners[i], NULL);
    }
    for (int i = 0; i < M; i++) {
        pthread_join(artillery[i], NULL);
    }

    free(banners);
    free(artillery);
    free(banner_args);
    free(artillery_args);

    sem_destroy(&shared.gorge_sem);
    pthread_mutex_destroy(&shared.hp_mutex);
    pthread_cond_destroy(&shared.charge_cv);
    pthread_barrier_destroy(&shared.art_barrier);

    return EXIT_SUCCESS;
}

