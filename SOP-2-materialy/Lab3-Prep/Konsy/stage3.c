#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHOP_FILENAME "./shop"
#define MIN_SHELVES 8
#define MAX_SHELVES 256
#define MIN_WORKERS 1
#define MAX_WORKERS 64

#define ERR(source)                                     \
    do                                                  \
    {                                                   \
        fprintf(stderr, "%s:%d\n", __FILE__, __LINE__); \
        perror(source);                                 \
        kill(0, SIGKILL);                               \
        exit(EXIT_FAILURE);                             \
    } while (0)

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
    pthread_mutex_t mxshelves[MAX_SHELVES];
    pthread_rwlock_t mxsorted;
    int sorted;
    int alive;
    pthread_mutex_t mxalive;
}worker_data;

void usage(char* program_name)
{
    fprintf(stderr, "Usage: \n");
    fprintf(stderr, "\t%s n m\n", program_name);
    fprintf(stderr, "\t  n - number of items (shelves), %d <= n <= %d\n", MIN_SHELVES, MAX_SHELVES);
    fprintf(stderr, "\t  m - number of workers, %d <= m <= %d\n", MIN_WORKERS, MAX_WORKERS);
    exit(EXIT_FAILURE);
}

void msleep(unsigned int milli)
{
    time_t sec = (int)(milli / 1000);
    milli = milli - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milli * 1000000L;
    if (nanosleep(&ts, &ts))
        ERR("nanosleep");
}

void shuffle(int* array, int n)
{
    for (int i = n - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        SWAP(array[i], array[j]);
    }
}

void print_array(int* array, int n)
{
    for (int i = 0; i < n; ++i)
    {
        printf("%3d ", array[i]);
    }
    printf("\n");
}

void child_work(int N, int* shelves, worker_data* data)
{   
    srand(getpid());
    printf("[%d] Worker reports for a night shift\n", getpid());
    while(1)
    {
        pthread_rwlock_rdlock(&data->mxsorted);
        if(data->sorted==1)
        {
            pthread_rwlock_unlock(&data->mxsorted);
            break;
        }
        pthread_rwlock_unlock(&data->mxsorted);

        int x = rand()%N;
        int y = rand()%N;
        
        while(x==y){y = rand()%N;}
        if(x>y)SWAP(x,y);
        
        int error;
        error = pthread_mutex_lock(&data->mxshelves[x]);
        if(error !=0)
        {
            if(error == EOWNERDEAD)
            {
                pthread_mutex_consistent(&data->mxshelves[x]);
                printf("[%d] Found a dead body in aisle [%d]\n", getpid(),x);
            }
        }
        error = pthread_mutex_lock(&data->mxshelves[y]);
        if(error !=0)
        {
            if(error == EOWNERDEAD)
            {
                pthread_mutex_consistent(&data->mxshelves[y]);
                printf("[%d] Found a dead body in aisle [%d]\n", getpid(),y);
            }
        }
        
        if(shelves[x]>shelves[y])
        {
            if(rand()%100==0)
            {
                printf("[%d] Trips over pallet and dies\n", getpid());
                pthread_mutex_lock(&data->mxalive);
                data->alive--;
                pthread_mutex_unlock(&data->mxalive);
                abort();
            }
            int temp = shelves[x];
            shelves[x] = shelves[y];
            shelves[y] = temp;
            msleep(100);
        }
        pthread_mutex_unlock(&data->mxshelves[y]);
        pthread_mutex_unlock(&data->mxshelves[x]);
    }
    exit(EXIT_SUCCESS);
}

void create_children(int M, int N, int* shelves, worker_data* data)
{
    for(int i = 0; i<M;i++)
    {
        pid_t pid;
        if((pid = fork())==-1)
            ERR("fork");
        if(pid == 0)
            child_work(N, shelves,data);
    }
}

void manager_work(int* shelves, int N,worker_data* data)
{
    printf("[%d] Manager reports for a night shift\n", getpid());
    while(data->sorted==0)
    {
        msleep(500);
        msync(shelves, sizeof(int)*N,MS_SYNC);
        pthread_rwlock_rdlock(&data->mxsorted);

        print_array(shelves, N);
        printf("[%d] Workers alive: [%d]\n", getpid(),data->alive);
        int sorted = 1;
        for(int i = 0; i<N-1;i++)
        {
            if(shelves[i]>shelves[i+1])
            {
                sorted = 0;
                break;
            }
        }
        pthread_rwlock_unlock(&data->mxsorted);

        if(sorted==1)
        {
            pthread_rwlock_wrlock(&data->mxsorted);
            data->sorted = 1;
            printf("[%d] The shop shelves are sorted\n", getpid());
            pthread_rwlock_unlock(&data->mxsorted);
        }
        pthread_mutex_lock(&data->mxalive);
        if(data->alive == 0)
        {
            printf("[%d] All workers died, I hate my job\n", getpid());
            pthread_mutex_unlock(&data->mxalive);
            exit(EXIT_SUCCESS);
        }
        pthread_mutex_unlock(&data->mxalive);
    }
    exit(EXIT_SUCCESS);
}

void create_manager(int* shelves, int N,worker_data* data)
{
    pid_t pid;
    if((pid=fork())==-1)
        ERR("fork");
    if(pid == 0)
        manager_work(shelves, N,data);
}

int main(int argc, char** argv)
{
    shm_unlink(SHOP_FILENAME);
    if(argc!=3)
        usage(argv[0]); 
    int N, M;
    N = atoi(argv[1]);
    M = atoi(argv[2]);

    if(N<8 || N>256 || M <1 || M>64)
        usage(argv[0]);

    int fd;
    if((fd = open(SHOP_FILENAME,O_CREAT | O_RDWR,0666))==-1)
        ERR("shm_open");
    if(ftruncate(fd, sizeof(int)*N)==-1)
        ERR("ftruncate");

    int* shelves;
    if((shelves = mmap(NULL, N * sizeof(int), PROT_READ | PROT_WRITE,MAP_SHARED, fd, 0))==MAP_FAILED)
        ERR("mmap");
    
    worker_data* data;
    if((data = mmap(NULL, sizeof(worker_data),PROT_READ | PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS, -1,0))==MAP_FAILED)
        ERR("mmap");

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);

    for(int i = 0; i<MAX_SHELVES;i++)
    {
        pthread_mutex_init(&data->mxshelves[i], &attr);
    }
    data->sorted = 0;
    data->alive = M;
    pthread_mutex_init(&data->mxalive,&attr);

    pthread_rwlockattr_t rwattr;

    pthread_rwlockattr_init(&rwattr);
    pthread_rwlockattr_setpshared(&rwattr, PTHREAD_PROCESS_SHARED);
    pthread_rwlock_init(&data->mxsorted, &rwattr);

    for(int i = 0; i<N;i++)
        shelves[i] = i+1;
    
    shuffle(shelves, N);
    print_array(shelves, N);

    create_children(M, N,shelves,data);

    create_manager(shelves,N,data);

    while(waitpid(0,NULL,0)>0){}

    print_array(shelves,N);
    printf("Night shift in Bitronka is over\n");

    pthread_mutexattr_destroy(&attr);
    for(int i = 0; i<MAX_SHELVES;i++)
        pthread_mutex_destroy(&data->mxshelves[i]);
    munmap(data, sizeof(worker_data));
    munmap(shelves, N * sizeof(int));
    shm_unlink(SHOP_FILENAME);
    return EXIT_SUCCESS;
}
