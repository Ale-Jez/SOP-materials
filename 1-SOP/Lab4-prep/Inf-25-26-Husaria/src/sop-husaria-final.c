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

typedef struct simulation_data
{
    int enemy_hp;
    int rain;
    int exit_flag;
    int cavalry_exit;
    int artillery_exit;

    sigset_t signals;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_barrier_t banners_barrier;
    pthread_barrier_t artillery_barrier;

    sem_t gorge_sem;
    sem_t supply_sem;
} simulation_data_t;

typedef struct thread_args
{
    simulation_data_t* data;
    unsigned int seed;
    pthread_t tid;
    int id;
} thread_args_t;

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

int random_number(unsigned int* seed, const int min, const int max) { return rand_r(seed) % (max - min + 1) + min; }

void* signal_thread(void* arg)
{
    thread_args_t* thread_args = arg;
    int sig;
    while (1)
    {
        if (sigwait(&thread_args->data->signals, &sig))
            ERR("sigwait");

        if (sig == SIGINT)
        {
            pthread_mutex_lock(&thread_args->data->mutex);
            thread_args->data->exit_flag = 1;
            pthread_cond_broadcast(&thread_args->data->cond);
            pthread_mutex_unlock(&thread_args->data->mutex);
            break;
        }
        if (sig == SIGUSR1)
        {
            pthread_mutex_lock(&thread_args->data->mutex);
            thread_args->data->rain = 1;
            pthread_mutex_unlock(&thread_args->data->mutex);
            puts("RAIN AND MUD IS SLOWING DOWN CHARGE\n");
        }
    }
    return NULL;
}

int check_exit(pthread_barrier_t* barrier, pthread_mutex_t* mutex, int* exit_flag, int* group_exit_flag)
{
    int serial = pthread_barrier_wait(barrier);
    if (serial == PTHREAD_BARRIER_SERIAL_THREAD)
    {
        pthread_mutex_lock(mutex);
        if (*exit_flag == 1)
            *group_exit_flag = 1;
        pthread_mutex_unlock(mutex);
    }
    pthread_barrier_wait(barrier);
    return *group_exit_flag;
}

void* artillery_thread(void* arg)
{
    thread_args_t* thread_args = arg;

    while (1)
    {
        int serial = pthread_barrier_wait(&thread_args->data->artillery_barrier);
        pthread_mutex_lock(&thread_args->data->mutex);
        if (thread_args->data->enemy_hp < 50)
        {
            if (serial == PTHREAD_BARRIER_SERIAL_THREAD)
                pthread_cond_broadcast(&thread_args->data->cond);
            pthread_mutex_unlock(&thread_args->data->mutex);
            break;
        }
        pthread_mutex_unlock(&thread_args->data->mutex);
        if (check_exit(&thread_args->data->artillery_barrier, &thread_args->data->mutex, &thread_args->data->exit_flag,
                       &thread_args->data->artillery_exit))
            break;
        pthread_mutex_lock(&thread_args->data->mutex);
        thread_args->data->enemy_hp -= random_number(&thread_args->seed, 1, 6);
        pthread_mutex_unlock(&thread_args->data->mutex);
        pthread_barrier_wait(&thread_args->data->artillery_barrier);
        pthread_mutex_lock(&thread_args->data->mutex);
        if (serial == PTHREAD_BARRIER_SERIAL_THREAD)
            printf("ARTILLERY: ENEMY HP %d\n", thread_args->data->enemy_hp);
        pthread_mutex_unlock(&thread_args->data->mutex);
        ms_sleep(400);
    }

    return NULL;
}

void* cavalry_thread(void* args)
{
    thread_args_t* thread_args = args;
    sem_wait(&thread_args->data->gorge_sem);
    ms_sleep(random_number(&thread_args->seed, 80, 120));
    sem_post(&thread_args->data->gorge_sem);
    printf("CAVALRY %d: IN POSITION\n", thread_args->id);

    pthread_mutex_lock(&thread_args->data->mutex);
    while (thread_args->data->enemy_hp >= 50 && thread_args->data->exit_flag == 0)
    {
        pthread_cond_wait(&thread_args->data->cond, &thread_args->data->mutex);
    }
    pthread_mutex_unlock(&thread_args->data->mutex);
    if (check_exit(&thread_args->data->banners_barrier, &thread_args->data->mutex, &thread_args->data->exit_flag,
                   &thread_args->data->cavalry_exit))
        return NULL;

    printf("CAVALRY %d: READY TO CHARGE\n", thread_args->id);

    for (int i = 0; i < 3; ++i)
    {
        if (check_exit(&thread_args->data->banners_barrier, &thread_args->data->mutex, &thread_args->data->exit_flag,
                       &thread_args->data->cavalry_exit))
            break;
        int serial = pthread_barrier_wait(&thread_args->data->banners_barrier);
        if (serial == PTHREAD_BARRIER_SERIAL_THREAD)
        {
            printf("CHARGE %d!\n", i + 1);
        }

        ms_sleep(500);
        pthread_mutex_lock(&thread_args->data->mutex);
        if (!thread_args->data->rain || random_number(&thread_args->seed, 1, 10) != 1)
        {
            thread_args->data->enemy_hp--;
        }
        else
        {
            printf("CAVALRY %d: MISSED\n", thread_args->id);
        }
        pthread_mutex_unlock(&thread_args->data->mutex);

        sem_wait(&thread_args->data->supply_sem);
        printf("CAVALRY %d: LANCE RESTOCKED\n", thread_args->id);
        ms_sleep(100);
        sem_post(&thread_args->data->supply_sem);
    }

    return NULL;
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        usage(argc, argv);
    }
    const int banners = atoi(argv[1]);
    if (banners < 10 || banners > 20)
    {
        usage(argc, argv);
    }
    const int artillery = atoi(argv[2]);
    if (artillery < 2 || artillery > 8)
    {
        usage(argc, argv);
    }

    srand(time(NULL));

    simulation_data_t data;
    data.enemy_hp = 100;
    data.exit_flag = 0;
    data.cavalry_exit = 0;
    data.artillery_exit = 0;
    data.rain = 0;
    sigemptyset(&data.signals);
    sigaddset(&data.signals, SIGINT);
    sigaddset(&data.signals, SIGUSR1);
    if (pthread_sigmask(SIG_BLOCK, &data.signals, NULL))
        ERR("pthread_sigmask");

    thread_args_t* thread_args = calloc(banners + artillery + 1, sizeof(thread_args_t));
    if (thread_args == NULL)
        ERR("calloc");

    if (sem_init(&data.gorge_sem, 0, 3))
        ERR("sem_init");
    if (sem_init(&data.supply_sem, 0, 4))
        ERR("sem_init");
    if (pthread_mutex_init(&data.mutex, NULL))
        ERR("pthread_mutex_init");
    if (pthread_cond_init(&data.cond, NULL))
        ERR("pthread_cond_init");
    if (pthread_barrier_init(&data.banners_barrier, NULL, banners))
        ERR("pthread_barrier_init");
    if (pthread_barrier_init(&data.artillery_barrier, NULL, artillery))
        ERR("pthread_barrier_init");

    for (int i = 0; i < banners; i++)
    {
        thread_args[i].id = i;
        thread_args[i].seed = rand();
        thread_args[i].data = &data;
        if (pthread_create(&thread_args[i].tid, NULL, cavalry_thread, &thread_args[i]))
            ERR("pthread_create");
    }

    for (int i = banners; i < banners + artillery; i++)
    {
        thread_args[i].seed = rand();
        thread_args[i].data = &data;
        if (pthread_create(&thread_args[i].tid, NULL, artillery_thread, &thread_args[i]))
            ERR("pthread_create");
    }

    thread_args[banners + artillery].data = &data;
    if (pthread_create(&thread_args[banners + artillery].tid, NULL, signal_thread, &thread_args[banners + artillery]))
        ERR("pthread_create");

    for (int i = 0; i < banners + artillery; i++)
    {
        if (pthread_join(thread_args[i].tid, NULL))
            ERR("pthread_join");
    }
    pthread_kill(thread_args[banners + artillery].tid, SIGINT);
    if (pthread_join(thread_args[banners + artillery].tid, NULL))
        ERR("pthread_join");

    printf("BATTLE ENDED. ENEMY HEALTH: %d\n", data.enemy_hp);
    if (data.enemy_hp <= 0)
        puts("VENIMUS, VIDIMUS, DEUS VICIT!");

    if (sem_destroy(&data.gorge_sem))
        ERR("sem_init");
    if (sem_destroy(&data.supply_sem))
        ERR("sem_init");
    if (pthread_mutex_destroy(&data.mutex))
        ERR("pthread_mutex_destroy");
    if (pthread_cond_destroy(&data.cond))
        ERR("pthread_cond_destroy");
    if (pthread_barrier_destroy(&data.banners_barrier))
        ERR("pthread_barrier_destroy");
    if (pthread_barrier_destroy(&data.artillery_barrier))
        ERR("pthread_barrier_destroy");
    free(thread_args);

    return EXIT_SUCCESS;
}
