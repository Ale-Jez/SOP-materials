#include "../include/inotify.h"

void add_to_map(WatchMap_t* map, int wd, const char* path)
{
    if (map->watch_count >= MAX_WATCHES)
        return;
    map->watch_map[map->watch_count].wd = wd;
    map->watch_map[map->watch_count].path = strdup(path);
    map->watch_count++;
}

Watch_t* find_watch(WatchMap_t* map, int wd)
{
    for (int i = 0; i < map->watch_count; i++)
    {
        if (map->watch_map[i].wd == wd)
            return &map->watch_map[i];
    }
    return NULL;
}

void remove_from_map(WatchMap_t* map, int wd)
{
    for (int i = 0; i < map->watch_count; i++)
    {
        if (map->watch_map[i].wd == wd)
        {
            free(map->watch_map[i].path);
            map->watch_map[i] = map->watch_map[map->watch_count - 1];
            map->watch_count--;
            return;
        }
    }
}

void update_watch_paths(WatchMap_t* map, const char* old_path, const char* new_path)
{
    size_t old_len = strlen(old_path);
    for (int i = 0; i < map->watch_count; i++)
    {
        if (strncmp(map->watch_map[i].path, old_path, old_len) == 0 &&
            (map->watch_map[i].path[old_len] == '/' || map->watch_map[i].path[old_len] == '\0'))
        {
            char new_full_path[PATH_MAX];
            snprintf(new_full_path, sizeof(new_full_path), "%s%s", new_path, map->watch_map[i].path + old_len);
            free(map->watch_map[i].path);
            map->watch_map[i].path = strdup(new_full_path);
        }
    }
}

void add_watch_recursive_monitor(int notify_fd, WatchMap_t* map, const char* base_path)
{
    uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF;
    int wd = inotify_add_watch(notify_fd, base_path, mask);
    if (wd >= 0)
        add_to_map(map, wd, base_path);

    DIR* dir = opendir(base_path);
    if (!dir)
        return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);
        struct stat st;
        if (lstat(full_path, &st) == 0 && S_ISDIR(st.st_mode))
        {
            add_watch_recursive_monitor(notify_fd, map, full_path);
        }
    }
    closedir(dir);
}
