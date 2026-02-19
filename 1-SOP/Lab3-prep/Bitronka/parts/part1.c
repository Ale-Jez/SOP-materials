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


typedef struct employee
{
    pthread_t tid;
}employee_t;


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

void* start_shift(void* arg){
    employee_t* argment = arg;
    printf("Worker %ld: Reporting for the night shift!\n", argment->tid);
    return NULL;
}

int main(int argc, char* argv[]) {
    if(argc != 3){
        usage(argc, argv);
    }
    int num_prod = atoi(argv[1]);
    int num_workers = atoi(argv[2]);
    if(num_prod < 8 || num_prod > 256 || num_workers < 1 || num_workers > 16){
        usage(argc, argv);
    }
    employee_t* workers = calloc(sizeof(employee_t), num_workers);
    for(int i = 0; i < num_workers; i++){
        pthread_create(&(workers[i].tid), NULL, start_shift, &workers[i]);
    }
    for(int i = 0; i < num_workers; i++){
        pthread_join(workers[i].tid, NULL);
    }
    free(workers);
}