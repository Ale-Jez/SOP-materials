#include "common.h"

//
#define MAX_WATCHES 8192
#define EVENT_BUF_LEN (64 * (sizeof(struct inotify_event) + NAME_MAX + 1))

typedef struct Watch_t
{
    int wd;
    char* path;
} Watch_t;

typedef struct WatchMap_t
{
    Watch_t watch_map[MAX_WATCHES];
    int watch_count;
} WatchMap_t;
