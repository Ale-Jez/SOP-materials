//#include <bits/pthreadtypes.h>
#include <math.h>
#include <bits/pthreadtypes.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include <threads.h>
#include <unistd.h>
#include <time.h>


#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef unsigned int UINT;
typedef struct timespec timespec_t;
void msleep(UINT milisec)
{
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    timespec_t req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if (nanosleep(&req, &req))
        ERR("nanosleep");
}


typedef struct thread_data{
    int id;
    float* task_array;
    float* result_array;
    int task_size;
    pthread_mutex_t * cell_mutex;
    pthread_mutex_t * counter_mutex;
    int* remaining;
}thread_data_t;


// Function to fill the task array with random floating-point numbers
void fill_task_array(float* array, int size) {
    for (int i = 0; i < size; i++) {
        array[i] = (float)(rand() % 6000) / 100.0 + 1.0; // Generates random numbers in range [1, 60]
    }
}



void print_array(int k, float* array, const char* name){
    printf("%s: ", name);
    for (int i = 0; i < k; i++){
        printf("%.2f ", array[i]);
    }
    printf("\n");
}


void* thread_function (void* arg){
    thread_data_t* data = (thread_data_t*)arg;
    //int random_index = rand() % data->task_size;
    unsigned int seed = time(NULL) * data->id; // Create a thread-specific seed


    while(1){
        if (pthread_mutex_lock(data->counter_mutex) != 0)
            ERR("mutex lock");

        if (*(data->remaining) <= 0){
            if (pthread_mutex_unlock(data->counter_mutex) != 0)
                ERR("mutex unlock");   
            break;
        }

        if (pthread_mutex_unlock(data->counter_mutex) != 0)
            ERR("mutex unlock");

        int random_index = rand_r(&seed) % data->task_size;

        if(pthread_mutex_trylock(&data->cell_mutex[random_index]) == 0)
        {
            if(data->result_array[random_index] == 0.0){
                float input = data->task_array[random_index];
                float result = sqrt(input);
                data->result_array[random_index] = result;

                if (pthread_mutex_lock(data->counter_mutex) != 0)
                    ERR("mutex lock");
                (*(data->remaining))--;
                if (pthread_mutex_unlock(data->counter_mutex) != 0)
                    ERR("mutex unlock");



                printf("Thread %d: sqrt(%.2f) = %.2f\n", data->id, input, result);
                //fflush(stdout);   
                msleep(100);

            }
            if (pthread_mutex_unlock(&data->cell_mutex[random_index]) != 0)
                ERR("mutex unlock");
        }
    }

    return NULL;
}




void usage(int argc, char* argv[])
{
    printf("%s n k\n", argv[0]);
    printf("\tn - the number of processing posix threads\n");
    printf("\tk - the task array size\n");
    exit(EXIT_FAILURE);
}






int main(int argc, char* argv[])
{
    if (argc != 3)
        usage(argc, argv);

    int n = atoi(argv[1]);
    if (n<0)
        ERR("Number of threads must be positive");
    
    int k = atoi(argv[2]);
    if (k<0)
        ERR("Size must be positive");

    srand(time(NULL));

    float* task_array = malloc(k* sizeof(float));
    if (!task_array)
        ERR("malloc");
    fill_task_array(task_array, k);

    print_array(k, task_array, "Task array");


    float* result_array = calloc(k, sizeof(float));
    if (!result_array)
        ERR("calloc");

    
    pthread_mutex_t* cell_mutex = calloc(k,sizeof(pthread_mutex_t));
    if (!cell_mutex)
        ERR("calloc");
    for (int i = 0; i < k; i++){
        if (pthread_mutex_init(&cell_mutex[i], NULL) != 0)
            ERR("mutex init");
    }



    int remaining = k;
    pthread_mutex_t counter_mutex;
    if (pthread_mutex_init(&counter_mutex, NULL) != 0)
        ERR("mutex init");



    pthread_t* threads = malloc ((n+1) * sizeof(pthread_t));
    if (threads == NULL)
        ERR("malloc");


    thread_data_t* thread_data = malloc ((n+1) * sizeof(thread_data_t));
    if (thread_data == NULL){
        free(threads);
        ERR("malloc");
    }

    for (int i = 0; i < n+1; i++){
        thread_data[i].id = i+1;
        thread_data[i].task_array = task_array;
        thread_data[i].result_array = result_array;
        thread_data[i].task_size = k;
        thread_data[i].cell_mutex = cell_mutex;
        thread_data[i].counter_mutex = &counter_mutex;
        thread_data[i].remaining = &remaining;

        if (pthread_create(&threads[i], NULL, thread_function, &thread_data[i]) != 0)
            ERR("pthread create");
    }

    // Wait for threads to complete
    for (int i = 0; i < n+1; i++){
        if (pthread_join(threads[i], NULL) != 0)
            ERR("pthread join");
    }
    print_array(k, result_array, "Result array");

    free(task_array);
    free(threads);
    free(thread_data);
    free(result_array);
    for (int i =0; i < k; i++){
        if (pthread_mutex_destroy(&cell_mutex[i])!= 0)
            ERR("mutex destroy");
    }
    free(cell_mutex);
    if (pthread_mutex_destroy(&counter_mutex)!= 0)
        ERR("mutex destroy");

    printf("\nAll threads completed.\n");
    exit(EXIT_SUCCESS);
}