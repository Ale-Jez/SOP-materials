#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

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
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

void msleep(unsigned int milisec)
{
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milisec * 1000000L;
    if (nanosleep(&ts, &ts))
        ERR("nanosleep");
}

void usage(int argc, char* argv[])
{
    printf("%s P Q\n", argv[0]);
    printf("\t1 <= P <= 10 - number of port workers\n");
    printf("\t1 <= Q <= 10 - number of carriage drivers\n");
    exit(EXIT_FAILURE);
}

struct timespec get_abs_wait_time(int milisec)
{
    static const long long billion = 1000000000;
    static const long long million = 1000000;
    struct timespec re;
    struct timeval now;
    gettimeofday(&now, NULL);
    re.tv_sec = now.tv_sec + (milisec / 1000);
    re.tv_nsec = now.tv_usec * 1000 + million * (milisec % 1000);
    re.tv_sec += re.tv_nsec / billion;
    re.tv_nsec %= billion;
    return re;
}
