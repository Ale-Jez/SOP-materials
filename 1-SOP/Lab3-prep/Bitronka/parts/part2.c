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
    unsigned int seed;
    int id;
    pthread_t tid;
    int* shelves;
    pthread_mutex_t* shelves_mx;
    int num_shelves;
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
    employee_t* argument = arg;
    printf("Worker %d: Reporting for the night shift!\n", argument->id);
    for(int i = 0; i < 10; i++){
        int product1 = rand_r(&argument->seed)%argument->num_shelves;
        int product2 = rand_r(&argument->seed)%argument->num_shelves;
        while(product1 == product2){
            product2 = rand_r(&argument->seed)%argument->num_shelves;
        }
        if(product1 > product2){
            SWAP(product1, product2);
        }
        pthread_mutex_lock(&argument->shelves_mx[product1]);
        pthread_mutex_lock(&argument->shelves_mx[product2]);
        if(argument->shelves[product2] < argument->shelves[product1]){
            SWAP(argument->shelves[product1], argument->shelves[product2]);
            ms_sleep(50);
        }
        pthread_mutex_unlock(&argument->shelves_mx[product1]);
        pthread_mutex_unlock(&argument->shelves_mx[product2]);
    }
    ms_sleep(100);
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
    int* shelves = calloc(sizeof(int), num_prod);
    pthread_mutex_t* shelves_mx = calloc(sizeof(pthread_mutex_t), num_prod);
    for(int i = 0; i < num_prod; i++){
        shelves[i] = i;
        pthread_mutex_init(&shelves_mx[i], NULL);
    }
    shuffle(shelves, num_prod);
    srand(time(NULL));
    print_shop(shelves, num_prod);
    for(int i = 0; i < num_workers; i++){
        workers[i].num_shelves = num_prod;
        workers[i].shelves = shelves;
        workers[i].shelves_mx = shelves_mx;
        workers[i].seed = rand();
        workers[i].id = i;
        if(pthread_create(&(workers[i].tid), NULL, start_shift, &workers[i]) != 0){
            ERR("pthread_create");
        }

    }
    for(int i = 0; i < num_workers; i++){
        if(pthread_join(workers[i].tid, NULL) != 0){
            ERR("pthread_join");
        }
    }
    print_shop(shelves, num_prod);
    free(workers);
    free(shelves);
    for(int i = 0; i<num_prod; i++){
        pthread_mutex_destroy(shelves_mx);
    }
    free(shelves_mx);
}