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

    sem_t suply_sem;  // Controls supply access
    pthread_barrier_t cav_barrier;  // Synchronizes cavalry charges

    int M ;
    int N ;
    volatile int is_raining;
} SharedData_t;

typedef struct {
    int id;
    SharedData_t *shared;
} ThreadArgs_t;


// --- Resources Wrapper for Cleanup ---
// We pass this to the signal thread so it can free memory on SIGINT
typedef struct {
    pthread_t *banners;
    pthread_t *artillery;
    ThreadArgs_t *banner_args;
    ThreadArgs_t *artillery_args;
    SharedData_t *shared;
    pthread_t sig_tid; // To allow main to join signal thread if needed
} Resources_t;


void cleanup_resources(Resources_t *res) {
    if (res->banners) free(res->banners);
    if (res->artillery) free(res->artillery);
    if (res->banner_args) free(res->banner_args);
    if (res->artillery_args) free(res->artillery_args);

    if (res->shared) {
        sem_destroy(&res->shared->gorge_sem);
        sem_destroy(&res->shared->suply_sem);
        pthread_mutex_destroy(&res->shared->hp_mutex);
        pthread_cond_destroy(&res->shared->charge_cv);
        pthread_barrier_destroy(&res->shared->art_barrier);
        pthread_barrier_destroy(&res->shared->cav_barrier);
    }
}

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


    for (int i = 0; i < 3; i++) {
        
        int res = pthread_barrier_wait(&shared->cav_barrier);
        if (res == PTHREAD_BARRIER_SERIAL_THREAD) {
            printf("CHARGE: %d\n", i + 1);
        }
        ms_sleep(500); 

        int missed =0;
        if (shared->is_raining) {
            // 10% chance to miss
            if ((rand() % 10) == 0) {
                missed = 1;
                printf("CAVALRY %d: MISSED\n", args->id);
            }
        }

        if (!missed) {
            if (pthread_mutex_lock(&shared->hp_mutex) != 0) ERR("pthread_mutex_lock");
            shared->enemy_hp--;
            if (pthread_mutex_unlock(&shared->hp_mutex) != 0) ERR("pthread_mutex_unlock");
        }
            
        //lance retrieval
        if (i < 2) { // No need to restock after final charge
            if (sem_wait(&shared->suply_sem) == -1) ERR("sem_wait");
            ms_sleep(100);
            if (sem_post(&shared->suply_sem) == -1) ERR("sem_post");
            printf("CAVALRY %d: LANCE RESTOCKED\n", args->id);
        }    
    }

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



// --- Stage 4: Signal Handler Thread ---
void* signal_thread_func(void* arg) {
    Resources_t* res = (Resources_t*)arg;
    sigset_t set;
    int sig;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGUSR1);

    while (1) {
        if (sigwait(&set, &sig) != 0) ERR("sigwait");

        if (sig == SIGUSR1) {
            printf("RAIN AND MUD IS SLOWING DOWN CHARGE\n");
            res->shared->is_raining = 1;
        } 
        else if (sig == SIGINT) {
            for (int i = 0; i < res->N; i++) pthread_cancel(res->banners[i]);
            for (int i = 0; i < res->M; i++) pthread_cancel(res->artillery[i]);

            for (int i = 0; i < res->N; i++) pthread_join(res->banners[i], NULL);
            for (int i = 0; i < res->M; i++) pthread_join(res->artillery[i], NULL);
            cleanup_resources(res);
            exit(EXIT_SUCCESS); 
        }
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

    // 1. Block signals BEFORE creating any threads
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGUSR1);
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) ERR("pthread_sigmask");


    // 2. Initialize Shared Data
    SharedData_t shared;
    shared.is_raining = 0; // Init rain flag
    shared.enemy_hp = 100;
    res.N = N; 
    res.M = M; 
    
    if (sem_init(&shared.gorge_sem, 0, 3) != 0) ERR("sem_init");
    if (pthread_mutex_init(&shared.hp_mutex, NULL) != 0) ERR("pthread_mutex_init");
    if (pthread_cond_init(&shared.charge_cv, NULL) != 0) ERR("pthread_cond_init");
    if (pthread_barrier_init(&shared.art_barrier, NULL, M) != 0) ERR("pthread_barrier_init");
    if (sem_init(&shared.suply_sem, 0, 4) != 0) ERR("sem_init");
    if (pthread_barrier_init(&shared.cav_barrier, NULL, N) != 0) ERR("pthread_barrier_init");

    // 3. Prepare Resources Struct (for signal thread to clean up)
    Resources_t res;
    res.shared = &shared;
    res.banners = calloc(sizeof(pthread_t), N);
    res.banner_args = calloc(sizeof(ThreadArgs_t), N);
    res.artillery = calloc(sizeof(pthread_t), M);
    res.artillery_args = calloc(sizeof(ThreadArgs_t), M);
    if (!res.banners || !res.banner_args || !res.artillery || !res.artillery_args) ERR("calloc");

    // 4. Create Signal Handling Thread
    pthread_t sig_thread;
    if (pthread_create(&sig_thread, NULL, signal_thread_func, &res) != 0) ERR("pthread_create");
    res.sig_tid = sig_thread;

    for (int i = 0; i < N; i++) {
        res.banner_args[i].id = i;
        res.banner_args[i].shared = &shared;
        if (pthread_create(&res.banners[i], NULL, cavalry_thread, &res.banner_args[i]) != 0) 
            ERR("pthread_create");
    }

    for (int i = 0; i < M; i++) {
        res.artillery_args[i].id = i;
        res.artillery_args[i].shared = &shared;
        if (pthread_create(&res.artillery[i], NULL, artillery_thread, &res.artillery_args[i]) != 0) 
            ERR("pthread_create");
    }

    for (int i = 0; i < N; i++) pthread_join(res.banners[i], NULL);
    for (int i = 0; i < M; i++) pthread_join(res.artillery[i], NULL);

    printf("BATTLE ENDED. ENEMY HEALTH: %d\n", shared.enemy_hp);
    if (shared.enemy_hp <= 0) {
        printf("VENIMUS, VIDIMUS, DEUS VICIT!\n");
    }

    pthread_kill(sig_thread, SIGINT);
    pthread_join(sig_thread, NULL);


    return EXIT_SUCCESS;
}
