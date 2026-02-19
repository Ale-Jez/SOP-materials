#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// taken and adpated from
// https://github.com/michau-s/OPS2-L1-Consultations

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)             \
    (__extension__({                               \
        long int __result;                         \
        do                                         \
            __result = (long int)(expression);     \
        while (__result == -1L && errno == EINTR); \
        __result;                                  \
    }))
#endif

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void msleep(int millisec)
{
    struct timespec tt;
    tt.tv_sec = millisec / 1000;
    tt.tv_nsec = (millisec % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

int count_descriptors()
{
    int count = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat stats;
    if ((dir = opendir("/proc/self/fd")) == NULL)
        ERR("opendir");
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    chdir("/proc/self/fd");
    do
    {
        errno = 0;
        if ((entry = readdir(dir)) != NULL)
        {
            if (lstat(entry->d_name, &stats))
                ERR("lstat");
            if (!S_ISDIR(stats.st_mode))
                count++;
        }
    } while (entry != NULL);
    if (chdir(path))
        ERR("chdir");
    if (closedir(dir))
        ERR("closedir");
    return count - 1;  // one descriptor for open directory
}

void usage(char* program)
{
    printf("%s w1 w2 w3\n", program);
    exit(EXIT_FAILURE);
}

void first_brigade_work(int production_pipe_write, int boss_pipe)
{
    srand(getpid());
    printf("Worker %d from the first brigade: descriptors: %d\n", getpid(), count_descriptors());
    close(production_pipe_write);
    close(boss_pipe);
}
void second_brigade_work(int production_pipe_write, int production_pipe_read, int boss_pipe)
{
    srand(getpid());
    printf("Worker %d from the second brigade: descriptors: %d\n", getpid(), count_descriptors());
    close(production_pipe_write);
    close(production_pipe_read);
    close(boss_pipe);
}
void third_brigade_work(int production_pipe_read, int boss_pipe)
{
    srand(getpid());
    printf("Worker %d from the third brigade: descriptors: %d\n", getpid(), count_descriptors());
    close(production_pipe_read);
    close(boss_pipe);
}

/*
void create_pipes_and_fork(int w1, int w2, int w3)
{


    int res = fork();
    if (res == -1) ERR("fork");
    if (res == 0)
    {
        for (int i = 0; i < w1; i++)
        {
            close(pipeBrig1B[2 * i]);  // Closing the reading end
            close(pipeProdBrig1[2 * i]);  // Closing the reading end

            first_brigade_work(pipeProdBrig1[2*i+1] , pipeBrig1B[2*i+1]);
            free(pipeBrig1B);
            free(pipeProdBrig1);
            exit(EXIT_SUCCESS);
        }

    }


    res = fork();
    if (res == -1) ERR("fork");
    if (res == 0)
    {
        for (int i = 0; i < w2; i++)
        {
            close(pipeBrig2B[2 * i]);  // Closing the BOSS reading end
            second_brigade_work(pipeProdBrig2[2*i+1], pipeProdBrig2[2*i], pipeBrig2B[2*i+1]);

            free(pipeBrig2B);
            free(pipeProdBrig2);
            exit(EXIT_SUCCESS);
        }

    }

    res = fork();
    if (res == -1) ERR("fork");
    if (res == 0)
    {
        for (int i = 0; i < w3; i++)
        {
            close(pipeBrig3B[2 * i]);  // Closing the reading end
            close(pipeProdBrig3[2 * i+1]);  // Closing the writing end
            third_brigade_work(pipeProdBrig3[2*i], pipeBrig3B[2*i+1]);
            free(pipeBrig3B);
            free(pipeProdBrig3);
            exit(EXIT_SUCCESS);
        }

    }


// Closing the pipes which remained open
    for (int i = 0; i < w1 * 2; i++)  // Times 2
    {
        close(pipeBrig1B[i]);
        close(pipeProdBrig1[i]);
    }
    for (int i = 0; i < w2 * 2; i++)  // Times 2
    {
        close(pipeBrig2B[i]);
        close(pipeProdBrig2[i]);
    }
    for (int i = 0; i < w3 * 2; i++)  // Times 2
    {
        close(pipeBrig3B[i]);
        close(pipeProdBrig3[i]);
    }
    free(pipeBrig1B);
    free(pipeProdBrig1);
    free(pipeBrig2B);
    free(pipeProdBrig2);
    free(pipeBrig3B);
    free(pipeProdBrig3);

}

*/

void create_first_brigade(int* pipeBoss, int* pipe12, int workersNum)
{
    int res;
    for (int i = 0; i < workersNum; i++)
    {
        res = fork();
        if (res == -1)
            ERR("fork");
        if (res == 0)
        {
            close(pipe12[0]);  // Closing the reading end
            for (int j = 0; j < workersNum; j++)
            {
                close(pipeBoss[2 * j]);
                if (j != i)
                    close(pipeBoss[2 * j + 1]);
            }

            first_brigade_work(pipe12[1], pipeBoss[2 * i + 1]);
            close(pipe12[1]);
            close(pipeBoss[2 * i + 1]);
            free(pipeBoss);
            exit(EXIT_SUCCESS);
        }
    }
}

void create_second_brigade(int* pipeBoss, int* pipe12, int* pipe23, int workersNum)
{
    int res;
    for (int i = 0; i < workersNum; i++)
    {
        res = fork();
        if (res == -1)
            ERR("fork");
        if (res == 0)
        {
            close(pipe12[1]);
            close(pipe23[0]);
            for (int j = 0; j < workersNum; j++)
            {
                close(pipeBoss[2 * j]);
                if (j != i)
                    close(pipeBoss[2 * j + 1]);
            }

            second_brigade_work(pipe12[1], pipe23[0], pipeBoss[2 * i + 1]);

            close(pipe12[0]);
            close(pipe23[1]);
            close(pipeBoss[2 * i + 1]);
            free(pipeBoss);
            exit(EXIT_SUCCESS);
        }
    }
}
void create_third_brigade(int* pipeBoss, int* pipe23, int workersNum)
{
    int res;
    for (int i = 0; i < workersNum; i++)
    {
        res = fork();
        if (res == -1)
            ERR("fork");
        if (res == 0)
        {
            close(pipe23[1]);
            for (int j = 0; j < workersNum; j++)
            {
                close(pipeBoss[2 * j]);
                if (j != i)
                    close(pipeBoss[2 * j + 1]);
            }

            third_brigade_work(pipe23[0], pipeBoss[2 * i + 1]);
            close(pipe23[1]);
            close(pipeBoss[2 * i + 1]);
            free(pipeBoss);
            exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc != 4)
        usage(argv[0]);
    int w1 = atoi(argv[1]), w2 = atoi(argv[2]), w3 = atoi(argv[3]);
    if (w1 < 1 || w1 > 10 || w2 < 1 || w2 > 10 || w3 < 1 || w3 > 10)
        usage(argv[0]);

    int* pipeBoss1;
    if ((pipeBoss1 = calloc(w1 * 2, sizeof(int))) == NULL)
        ERR("calloc");
    for (int i = 0; i < w1; i++)
    {
        if (pipe(&pipeBoss1[2 * i]) == -1)
            ERR("pipe");
    }
    for (int i = 0; i < w1; i++)
    {
        close(pipeBoss1[2 * i + 1]);
    }
    int* pipeBoss2;
    if ((pipeBoss2 = calloc(w2 * 2, sizeof(int))) == NULL)
        ERR("calloc");
    for (int i = 0; i < w1; i++)
    {
        if (pipe(&pipeBoss2[2 * i]) == -1)
            ERR("pipe");
    }
    for (int i = 0; i < w2; i++)
    {
        close(pipeBoss2[2 * i + 1]);
    }
    int* pipeBoss3;
    if ((pipeBoss3 = calloc(w3 * 2, sizeof(int))) == NULL)
        ERR("calloc");
    for (int i = 0; i < w3; i++)
    {
        if (pipe(&pipeBoss3[2 * i]) == -1)
            ERR("pipe");
    }
    for (int i = 0; i < w3; i++)
    {
        close(pipeBoss3[2 * i + 1]);
    }

    int pipe12[2];
    if (pipe(pipe12) == -1)
        ERR("pipe");
    int pipe23[2];
    if (pipe(pipe23) == -1)
        ERR("pipe");

    create_first_brigade(pipeBoss1, pipe12, w1);
    create_second_brigade(pipeBoss2, pipe12, pipe23, w2);
    create_third_brigade(pipeBoss3, pipe23, w3);

    while (wait(NULL) > 0)
        ;

    printf("Boss: descriptors: %d\n", count_descriptors());

    for (int i = 0; i < w1 * 2; i++)
        close(pipeBoss1[i]);
    free(pipeBoss1);
    for (int i = 0; i < w2 * 2; i++)
        close(pipeBoss2[i]);
    free(pipeBoss2);
    for (int i = 0; i < w3 * 2; i++)
        close(pipeBoss3[i]);
    free(pipeBoss3);
    close(pipe12[0]);
    close(pipe12[1]);
    close(pipe23[0]);
    close(pipe23[1]);
}
