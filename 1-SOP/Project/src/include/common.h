#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

//
#define MAX_INPUT_LEN 4096
#define MAX_ARGS 64
#define MAX_BACKUPS 100
#define MAX_BUF 4096

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

//

typedef struct child_info_t
{
    pid_t pid;
    char* source_path;
    char* target_path;

} child_info_t;

typedef struct children_data_t
{
    child_info_t* children;
    int count;
} children_data_t;

//
//

void sethandler(void (*f)(int), int sigNo);
void ms_sleep(unsigned int milli);

#endif
