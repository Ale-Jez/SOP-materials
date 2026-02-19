#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define CANDIDATES 256

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

/**
 * @struct queue
 * @brief Data structure to represent a thread-safe queue.
 */
struct queue
{
    char** paths;           /**< Array to store paths in the queue. */
    int head;               /**< Index of the head of the queue. */
    int tail;               /**< Index of the tail of the queue. */
    int count;              /**< Current number of elements in the queue. */
    int size;               /**< Maximum size of the queue. */
    int completed_adding;   /**< Flag to indicate all paths have been added to the queue. */
    pthread_mutex_t mutex;  // "You may add one new field [...] to ensure thread-safe access"
};

struct threadArgs
{
    struct queue* queue;
    pthread_t id;
    pthread_mutex_t* mutexes;  // "Each field of this array is protected by a dedicated mutex"
    int* votes;                // "Threads count votes into a shared array"

    // Signal handling
    sigset_t* pMask;
    int* finished;
    pthread_mutex_t* finished_mutex;

} typedef threadArgs_t;

/**
 * @brief Initializes a queue.
 *
 * Allocates memory for a new queue instance and initializes its members.
 *
 * @param size The maximum number of paths the queue can hold.
 * @return Pointer to the initialized queue.
 */
struct queue* queue_init(int size)
{
    // Initializing every field of the queue
    struct queue* q;

    if ((q = malloc(sizeof(struct queue))) == NULL)
        ERR("malloc");

    char** p;

    if ((p = calloc(size, sizeof(char*))) == NULL)
        ERR("calloc");

    q->size = size;
    q->count = 0;
    q->completed_adding = 0;
    q->head = 0;
    q->tail = 0;
    q->paths = p;

    pthread_mutex_init(&q->mutex, NULL);

    return q;
}

/**
 * @brief Adds a path to the queue.
 *
 * Waits 50ms if the queue is full until space becomes available (busy-wait loop).
 * Returns 0 if queue_complete_adding was called, 1 if the item was successfully added.
 *
 * @note This function duplicates the input string using `strdup`. The caller
 *       is responsible for freeing the memory of the input string if necessary,
 *       but the queue manages its own storage for paths.
 *
 * @param queue Pointer to the queue.
 * @param path Task to add to the queue (a string).
 * @return Flag if the operation was successful.
 */
int queue_enqueue(struct queue* queue, char* path)
{
    if (pthread_mutex_lock(&queue->mutex))
        ERR("mutex lock");

    // If the queue is full we wait
    while (queue->count == queue->size && queue->completed_adding == 0)
    {
        if (pthread_mutex_unlock(&queue->mutex))
            ERR("mutex unlock");

        msleep(50);

        if (pthread_mutex_lock(&queue->mutex))
            ERR("mutex lock");
    }

    // If the adding was completed we return
    if (queue->completed_adding == 1)
    {
        if (pthread_mutex_unlock(&queue->mutex))
            ERR("mutex unlock");
        return 0;
    }

    // Allocating paths and adding a path to the queue
    queue->paths[queue->tail] = strdup(path);
    queue->tail++;
    if (queue->tail >= queue->size)
        queue->tail = 0;
    queue->count++;

    if (pthread_mutex_unlock(&queue->mutex))
        ERR("mutex unlock");

    return 1;
}

/**
 * @brief Marks the queue as no longer accepting new paths.
 *
 * This indicates that no more paths will be added to the queue.
 *
 * @param queue Pointer to the queue.
 */
void queue_complete_adding(struct queue* queue)
{
    if (pthread_mutex_lock(&queue->mutex))
        ERR("mutex lock");

    queue->completed_adding = 1;

    if (pthread_mutex_unlock(&queue->mutex))
        ERR("mutex lock");
}

/**
 * @brief Removes a path from the queue.
 *
 * Waits 5ms if the queue is empty until a new path is available. (busy-wait loop).
 * Returns NULL if no paths are available and queue_complete_adding was called.
 *
 * @param queue Pointer to the queue.
 * @return Pointer to the path string. NULL if no paths are available and adding is completed.
 */
char* queue_dequeue(struct queue* queue)
{
    if (pthread_mutex_lock(&queue->mutex))
        ERR("mutex lock");

    // If the queue is empty we wait
    while (queue->count == 0 && queue->completed_adding == 0)
    {
        if (pthread_mutex_unlock(&queue->mutex))
            ERR("mutex unlock");

        msleep(5);

        if (pthread_mutex_lock(&queue->mutex))
            ERR("mutex lock");
    }
    // If the adding was completed we return
    if (queue->completed_adding == 1 && queue->count == 0)
    {
        if (pthread_mutex_unlock(&queue->mutex))
            ERR("mutex unlock");

        return NULL;
    }

    // Removing the element
    char* p = queue->paths[queue->head];
    // Asign NULL after removing to ensure no problems with free() later
    queue->paths[queue->head] = NULL;
    queue->head = (queue->head + 1) % queue->size;
    queue->count--;

    if (pthread_mutex_unlock(&queue->mutex))
        ERR("mutex unlock");

    return p;
}

/**
 * @brief Cleans up the queue and releases all resources.
 *
 * Frees memory allocated for the paths and the queue structure.
 * Destroys the mutex associated with the queue.
 *
 * @param queue Pointer to the queue to be disposed.
 */
void queue_dispose(struct queue* queue)
{
    // We may assume only one thread will call this function (same with init)

    for (int i = 0; i < queue->size; i++)
    {
        // Freeing NULL is safe
        free(queue->paths[i]);
    }

    free(queue->paths);

    if (pthread_mutex_destroy(&queue->mutex))
        ERR("mutex destroy");

    free(queue);
}

ssize_t bulk_read(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (c == 0)
            return len;  // EOF
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

void scan_dir(char* p, struct queue* q)
{
    DIR* dirp;
    struct dirent* dp;
    struct stat filestat;

    if ((dirp = opendir(p)) == NULL)
        ERR("opendir");

    do
    {
        errno = 0;
        if ((dp = readdir(dirp)) != NULL)
        {
            // "The main thread scans directory p for files and places their paths into a queue"
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s%s", p, dp->d_name);

            if (lstat(path, &filestat))
                ERR("lstat");

            if (S_ISREG(filestat.st_mode))
            {
                if (!queue_enqueue(q, path))
                {
                    break;
                }
            }
        }
    } while (dp != NULL);

    if (errno != 0)
        ERR("readdir");

    if (closedir(dirp))
        ERR("closedir");
}

void print_votes(int votes[CANDIDATES])
{
    for (int i = 0; i < CANDIDATES; i++)
        printf("%3d ", votes[i]);
    printf("\n");
}

void* start_routine(void* arg)
{
    threadArgs_t* args = (threadArgs_t*)(arg);
    char* path;

    while ((path = queue_dequeue(args->queue)))
    {
        // "Worker threads retrieve paths from the queue and print them along with their id"
        printf("Thread[%lu] %s\n", (unsigned long)pthread_self(), path);

        int fd;

        if ((fd = open(path, O_RDONLY)) == -1)
            ERR("open");

        // "Votes are read in batches into a buffer of size 256 bytes"
        char buff[256];
        int read;

        while (1)
        {
            if (pthread_mutex_lock(args->finished_mutex))
                ERR("mutex lock");

            // If we finished counting earlier, break
            if (*(args->finished))
            {
                if (pthread_mutex_unlock(args->finished_mutex))
                    ERR("mutex unlock");
                break;
            }

            if (pthread_mutex_unlock(args->finished_mutex))
                ERR("mutex unlock");

            if ((read = bulk_read(fd, buff, sizeof(buff))) == -1)
                ERR("read");

            if (read == 0)
                break;

            for (int i = 0; i < read; i++)
            {
                if (pthread_mutex_lock(&args->mutexes[i]))
                    ERR("mutex lock");

                args->votes[(unsigned char)buff[i]]++;

                msleep(1);

                if (pthread_mutex_unlock(&args->mutexes[i]))
                    ERR("mutex unlock");
            }
        }

        if (close(fd))
            ERR("close");

        free(path);

        if (pthread_mutex_lock(args->finished_mutex))
            ERR("mutex lock");

        // If we finished counting earlier, break
        if (*(args->finished))
        {
            if (pthread_mutex_unlock(args->finished_mutex))
                ERR("mutex unlock");
            break;
        }

        if (pthread_mutex_unlock(args->finished_mutex))
            ERR("mutex unlock");
    }

    return NULL;
}

// Start routine for the thread responsible for handling signals
void* start_routine_signal(void* arg)
{
    threadArgs_t* args = (threadArgs_t*)(arg);
    int signo;
    srand(time(NULL));
    for (;;)
    {
        if (sigwait(args->pMask, &signo))
            ERR("sigwait failed.");
        switch (signo)
        {
            // "Receiving SIGINT signifies an immediate termination of the voting process"
            case SIGINT:

                if (pthread_mutex_lock(args->finished_mutex))
                    ERR("mutex lock");

                *(args->finished) = 1;

                queue_complete_adding(args->queue);

                if (pthread_mutex_unlock(args->finished_mutex))
                    ERR("mutex unlock");

                return NULL;

            // "Handling SIGUSR1 will involve printing partial voting results"
            case SIGUSR1:

                for (int i = 0; i < 256; i++)
                {
                    if (pthread_mutex_lock(&args->mutexes[i]))
                        ERR("mutex lock");
                }

                print_votes(args->votes);

                for (int i = 0; i < 256; i++)
                {
                    if (pthread_mutex_unlock(&args->mutexes[i]))
                        ERR("mutex unlock");
                }

                break;

            default:
                printf("unexpected signal %d\n", signo);
                exit(1);
        }
    }

    return NULL;
}

void usage(int argc, char* argv[])
{
    printf("%s p q t\n", argv[0]);
    printf("\tp - path to file to be encrypted\n");
    printf("\t1 <= q <= 8 - size of paths queue\n");
    printf("\t1 <= t <= 16 - number of processing threads\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
    // Usage
    if (argc != 4)
        usage(argc, argv);

    char* p = argv[1];
    int q = atoi(argv[2]);
    int t = atoi(argv[3]);

    if (t < 1 || t > 16)
        usage(argc, argv);

    if (q < 1 || q > 8)
        usage(argc, argv);

    // Signal Handling
    sigset_t newMask;
    sigemptyset(&newMask);
    sigaddset(&newMask, SIGINT);
    sigaddset(&newMask, SIGUSR1);
    if (pthread_sigmask(SIG_BLOCK, &newMask, NULL))
        ERR("SIG_BLOCK error");

    struct queue* queue = queue_init(q);
    threadArgs_t* args = calloc(t + 1, sizeof(threadArgs_t));  // One additional thread for signal handling

    // Array for counting votes
    int votes[256] = {0};
    pthread_mutex_t mutexes[256];

    // For signal handling
    pthread_mutex_t finished_mut = PTHREAD_MUTEX_INITIALIZER;
    int finished = 0;

    for (int i = 0; i < 256; i++)
    {
        if (pthread_mutex_init(&mutexes[i], NULL))
            ERR("mutex init");
    }

    // Creating worker threads
    for (int i = 0; i < t + 1; i++)  // One additional thread for signal handling
    {
        // Initializing threadArgs struct
        args[i].queue = queue;
        args[i].mutexes = mutexes;
        args[i].votes = votes;
        args[i].finished_mutex = &finished_mut;
        args[i].finished = &finished;
        args[i].pMask = &newMask;

        pthread_create(&args[i].id, NULL, start_routine, &args[i]);
    }

    // Creating signal handling thread
    pthread_create(&args[t].id, NULL, start_routine_signal, &args[t]);

    // Main thread work
    scan_dir(p, queue);
    queue_complete_adding(queue);

    for (int i = 0; i < t; i++)
    {
        pthread_join(args[i].id, NULL);
    }

    kill(getpid(), SIGINT);
    pthread_join(args[t].id, NULL);

    for (int i = 0; i < 256; i++)
    {
        if (pthread_mutex_destroy(&mutexes[i]))
            ERR("mutex destroy");
    }

    print_votes(votes);

    free(args);
    queue_dispose(queue);

    exit(EXIT_SUCCESS);
}
