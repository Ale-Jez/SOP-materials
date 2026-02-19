#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define SWAP(x, y)         \
    do                     \
    {                      \
        typeof(x) __x = x; \
        typeof(y) __y = y; \
        x = __y;           \
        y = __x;           \
    } while (0)

typedef struct
{
    pthread_t thread;
    int id;
    unsigned int seed;
    int* shop;
    pthread_mutex_t* shop_mutexes;
    int n;
    int* do_work;
    pthread_mutex_t* do_work_mutex;
    int cancelled;
} WorkerArgs;

typedef struct
{
    pthread_t thread;
    int* shop;
    pthread_mutex_t* shop_mutexes;
    int n;
    WorkerArgs* workers;
    int m;
    int* do_work;
    pthread_mutex_t* do_work_mutex;
    sigset_t* mask;
} SignalArgs;

void usage(int argc, char* argv[])
{
    printf("%s N M\n", argv[0]);
    printf("\t8 <= N <= 256 - number of products\n");
    printf("\t1 <= M <= 16 - number of workers\n");
    exit(EXIT_FAILURE);
}

void shuffle(int* shop, int n)
{
    for (int i = n - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        SWAP(shop[i], shop[j]);
    }
}

void print_shop(int* shop, int n)
{
    for (int i = 0; i < n; i++)
        printf("%3d ", shop[i]);
    printf("\n");
}

void ms_sleep(unsigned int milli)
{
    time_t sec = (int)(milli / 1000);
    milli = milli - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milli * 1000000L;
    if (nanosleep(&ts, &ts))
        ERR("nanosleep");
}

void release_mutex(void* mutex) { pthread_mutex_unlock(mutex); }

void* worker_func(void* void_args)
{
    // These are default:
    // pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    // pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    WorkerArgs* args = void_args;
    printf("Worker %d: Reporting for night shift\n", args->id);
    while (1)
    {
        pthread_mutex_lock(args->do_work_mutex);
        if (*args->do_work == 0)
        {
            pthread_mutex_unlock(args->do_work_mutex);
            break;
        }
        pthread_mutex_unlock(args->do_work_mutex);

        int i = rand_r(&(args->seed)) % args->n;
        int j = rand_r(&(args->seed)) % args->n;
        while (i == j)
            j = rand_r(&(args->seed)) % args->n;

        if (i > j)
            SWAP(i, j);

        ms_sleep(100);

        pthread_mutex_lock(&args->shop_mutexes[i]);
        pthread_mutex_lock(&args->shop_mutexes[j]);

        pthread_cleanup_push(release_mutex, (void*)&args->shop_mutexes[i]);
        pthread_cleanup_push(release_mutex, (void*)&args->shop_mutexes[j]);

        ms_sleep(50);
        if (args->shop[i] > args->shop[j])
        {
            SWAP(args->shop[i], args->shop[j]);
        }

        pthread_cleanup_pop(1);
        pthread_cleanup_pop(1);
    }

    return NULL;
}

void* signal_func(void* void_args)
{
    alarm(1);
    SignalArgs* args = void_args;
    int signal = 0;
    int worker;
    int cancelled = 0;
    while (1)
    {
        sigwait(args->mask, &signal);
        switch (signal)
        {
            case SIGINT:
                pthread_mutex_lock(args->do_work_mutex);
                *args->do_work = 0;
                pthread_mutex_unlock(args->do_work_mutex);
                return NULL;
            case SIGALRM:
                for (int i = 0; i < args->n; ++i)
                {
                    pthread_mutex_lock(&args->shop_mutexes[i]);
                }
                print_shop(args->shop, args->n);
                for (int i = 0; i < args->n; ++i)
                {
                    pthread_mutex_unlock(&args->shop_mutexes[i]);
                }
                alarm(1);
                break;
            case SIGUSR1:
                for (int i = 0; i < args->n; ++i)
                {
                    pthread_mutex_lock(&args->shop_mutexes[i]);
                }
                shuffle(args->shop, args->n);
                for (int i = 0; i < args->n; ++i)
                {
                    pthread_mutex_unlock(&args->shop_mutexes[i]);
                }
                break;
            case SIGUSR2:
                worker = rand() % args->m;
                for (int i = 0; i < args->m; ++i)
                {
                    int index = (worker + i) % args->m;
                    if (!args->workers[index].cancelled)
                    {
                        args->workers[index].cancelled = 1;
                        printf("Cancelled worker %d\n", index);
                        pthread_cancel(args->workers[index].thread);
                        break;
                    }
                }
                if (++cancelled == args->m)
                {
                    printf("Cancelled all workers\n");
                    return NULL;
                }
                break;
            default:
                printf("Unexpected signal: %d\n", signal);
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc != 3)
        usage(argc, argv);

    srand(time(NULL));

    int n, m;
    n = atoi(argv[1]);
    m = atoi(argv[2]);

    if (n < 8 || n > 256 || m < 1 || m > 16)
        usage(argc, argv);

    int do_work = 1;
    pthread_mutex_t do_work_mutex = PTHREAD_MUTEX_INITIALIZER;

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGALRM);
    if (pthread_sigmask(SIG_BLOCK, &mask, NULL))
        ERR("pthread_sigmask");

    int* shop = calloc(n, sizeof(int));
    if (!shop)
        ERR("calloc");
    pthread_mutex_t* shop_mutexes = calloc(n, sizeof(pthread_mutex_t));
    if (!shop_mutexes)
        ERR("calloc");

    for (int i = 0; i < n; ++i)
    {
        if (pthread_mutex_init(&shop_mutexes[i], NULL))
            ERR("pthread_mutex_init");
        shop[i] = i + 1;
    }
    shuffle(shop, n);

    WorkerArgs* worker_args = calloc(m, sizeof(WorkerArgs));
    if (!worker_args)
        ERR("calloc");

    SignalArgs signal_args;

    for (int i = 0; i < m; ++i)
    {
        worker_args[i].shop = shop;
        worker_args[i].shop_mutexes = shop_mutexes;
        worker_args[i].do_work_mutex = &do_work_mutex;
        worker_args[i].n = n;
        worker_args[i].seed = rand();
        worker_args[i].do_work = &do_work;
        worker_args[i].id = i;
        worker_args[i].cancelled = 0;
        if (pthread_create(&worker_args[i].thread, NULL, worker_func, &worker_args[i]))
            ERR("pthread_create");
    }

    signal_args.n = n;
    signal_args.m = m;
    signal_args.workers = worker_args;
    signal_args.shop_mutexes = shop_mutexes;
    signal_args.do_work_mutex = &do_work_mutex;
    signal_args.do_work = &do_work;
    signal_args.shop = shop;
    signal_args.mask = &mask;
    if (pthread_create(&signal_args.thread, NULL, signal_func, &signal_args))
        ERR("pthread_create");

    for (int i = 0; i < m; ++i)
    {
        if (pthread_join(worker_args[i].thread, NULL))
            ERR("pthread_join");
    }
    if (pthread_join(signal_args.thread, NULL))
        ERR("pthread_join");

    for (int i = 0; i < n; ++i)
    {
        if (pthread_mutex_destroy(&shop_mutexes[i]))
            ERR("pthread_mutex_destroy");
    }

    free(shop);
    free(shop_mutexes);
    free(worker_args);

    exit(EXIT_SUCCESS);
}
