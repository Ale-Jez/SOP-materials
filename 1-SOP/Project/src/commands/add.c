#include "../include/common.h"
#include "../include/functions.h"
#include "../include/inotify.h"

//
static void add_command(char** args);

// from inotify.c
Watch_t* find_watch(WatchMap_t* map, int wd);
void remove_from_map(WatchMap_t* map, int wd);
void update_watch_paths(WatchMap_t* map, const char* old_path, const char* new_path);
void add_watch_recursive_monitor(int notify_fd, WatchMap_t* map, const char* path);

//

void add_manage(char** args, children_data_t* data, int argCount)
{
    if (strcmp(args[0], "add") != 0)
        return;

    if (argCount < 3)
    {
        printf("Error missing paths\n");
        return;
    }

    char* source_raw = args[1];
    char source_abs[PATH_MAX];
    if (!realpath(source_raw, source_abs))
    {
        printf("Error resolving source path\n");
        return;
    }

    // Loop through all target paths
    for (int i = 2; i < argCount; i++)
    {
        char* target_raw = args[i];

        // Prevent directory inside itself
        char cwd[PATH_MAX];
        if (getcwd(cwd, PATH_MAX) == NULL)
            ERR("getcwd");

        char target_abs_est[PATH_MAX];
        int written;
        if (target_raw[0] == '/')
            written = snprintf(target_abs_est, PATH_MAX, "%s", target_raw);
        else
            written = snprintf(target_abs_est, PATH_MAX, "%s/%s", cwd, target_raw);

        if (written < 0 || written >= PATH_MAX)
        {
            printf("Error Target path is too long (truncated)\n");
            continue;
        }

        if (is_subpath(source_abs, target_abs_est) || is_subpath(target_abs_est, source_abs))
        {
            printf("Error Cannot backup directory into itself or parent (%s -> %s)\n", source_abs, target_abs_est);
            continue;
        }

        // Check for duplicates
        int duplicate = 0;
        for (int j = 0; j < data->count; j++)
        {
            if (strcmp(data->children[j].source_path, source_abs) == 0 &&
                strcmp(data->children[j].target_path, target_abs_est) == 0)
            {
                printf("Error Backup already exists for %s -> %s\n", source_abs, target_abs_est);
                duplicate = 1;
                break;
            }
        }
        if (duplicate == 1)
            continue;

        pid_t pid = fork();
        if (pid == -1)
            ERR("fork");
        else if (pid == 0)  // Child
        {
            sethandler(SIG_DFL, SIGINT);
            sethandler(SIG_DFL, SIGTERM);

            // Create a specific argument list for the child function
            char* child_args[] = {"add", source_abs, target_abs_est};
            add_command(child_args);
            exit(EXIT_SUCCESS);
        }
        else  // Parent
        {
            data->count++;
            data->children = realloc(data->children, data->count * sizeof(child_info_t));
            if (data->children == NULL)
                ERR("realloc");

            data->children[data->count - 1].pid = pid;
            data->children[data->count - 1].source_path = strdup(source_abs);
            data->children[data->count - 1].target_path = strdup(target_abs_est);
        }
    }
}

// --------------------------------------------------
// child's function
// --------------------------------------------------

static void add_command(char** args)
{
    char* source = args[1];
    char* target = args[2];

    // Check target state
    struct stat st;
    if (stat(target, &st) == -1)
    {
        if (errno == ENOENT)
        {
            if (mkdir(target, 0755) == -1)
                ERR("mkdir target");
        }
        else
        {
            printf("stat target\n");
            return;
        }
    }
    else
    {
        if (!S_ISDIR(st.st_mode))
        {
            printf("Error Target is not a directory\n");
            printf("> ");
            return;
        }
        if (is_dir_empty(target) != 1)
        {
            printf("Error Target directory %s is not empty\n", target);
            printf("> ");
            return;
        }
    }

    // Initial backup
    copy_recursive(source, target, source, target);

    // Inotify inspired from:
    /*
        https://gitlab.com/SaQQ/sop1/-/tree/master/05_events?ref_type=heads

    especially from:
        https://gitlab.com/SaQQ/sop1/-/blob/master/05_events/watch_dir.c?ref_type=heads
        https://gitlab.com/SaQQ/sop1/-/blob/master/05_events/watch_file.c?ref_type=heads
        https://gitlab.com/SaQQ/sop1/-/blob/master/05_events/watch_tree.c?ref_type=heads
    */

    // Setup Inotify
    int notify_fd = inotify_init();
    if (notify_fd < 0)
        ERR("inotify_init");

    WatchMap_t map = {0};
    add_watch_recursive_monitor(notify_fd, &map, source);

    uint32_t pending_cookie = 0;
    char pending_move_src[PATH_MAX] = "";

    // Monitoring loop
    while (map.watch_count > 0)
    {
        char buffer[EVENT_BUF_LEN];
        ssize_t len = read(notify_fd, buffer, EVENT_BUF_LEN);
        if (len < 0)
        {
            if (errno == EINTR)
                continue;
            ERR("read");
        }

        ssize_t i = 0;
        while (i < len)
        {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];
            Watch_t* watch = find_watch(&map, event->wd);

            // Construct full absolute path of the event source
            char event_src_path[PATH_MAX] = "";
            if (watch)
            {
                if (event->len > 0)  // File inside watched directory
                    snprintf(event_src_path, sizeof(event_src_path), "%s/%s", watch->path, event->name);
                else
                    strncpy(event_src_path, watch->path, sizeof(event_src_path));
            }

            // Determine corresponding target path
            char event_dst_path[PATH_MAX] = "";
            if (watch && strlen(event_src_path) > 0)
            {
                // Calculate relative path from source root
                if (strlen(event_src_path) >= strlen(source))
                {
                    const char* rel = event_src_path + strlen(source);
                    snprintf(event_dst_path, sizeof(event_dst_path), "%s%s", target, rel);
                }
            }

            if (event->mask & IN_IGNORED)
                remove_from_map(&map, event->wd);

            else if (strlen(event_dst_path) > 0)
            {
                // Handle pending move
                // if we had a FROM but no matching TO, we must treat it as delete now
                if (pending_cookie != 0 && event->cookie != pending_cookie)
                {
                    // Previous move was incomplete (outside watch), delete old
                    char pending_dst[PATH_MAX];
                    const char* rel = pending_move_src + strlen(source);
                    snprintf(pending_dst, PATH_MAX, "%s%s", target, rel);
                    remove_recursive(pending_dst);
                    pending_cookie = 0;
                    pending_move_src[0] = '\0';
                }

                if (event->mask & IN_CREATE)
                {
                    if (event->mask & IN_ISDIR)
                    {
                        mkdir(event_dst_path, 0755);
                        // watch new dir
                        add_watch_recursive_monitor(notify_fd, &map, event_src_path);
                        // sync files
                        copy_recursive(event_src_path, event_dst_path, source, target);
                    }
                    else
                        copy_item(event_src_path, event_dst_path, source, target);
                }

                else if (event->mask & IN_MODIFY)
                {
                    if (!(event->mask & IN_ISDIR))
                        copy_item(event_src_path, event_dst_path, source, target);
                }

                else if (event->mask & IN_DELETE)
                    remove_recursive(event_dst_path);

                else if (event->mask & IN_DELETE_SELF)
                {
                    // Source root deleted
                    if (strcmp(event_src_path, source) == 0)
                    {
                        close(notify_fd);
                        exit(EXIT_SUCCESS);
                    }
                }

                // start of renaming / moving
                else if (event->mask & IN_MOVED_FROM)
                {
                    pending_cookie = event->cookie;
                    strncpy(pending_move_src, event_src_path, sizeof(pending_move_src));
                }

                // second part of renaming / moving
                else if (event->mask & IN_MOVED_TO)
                {
                    if (event->cookie == pending_cookie && pending_cookie != 0)
                    {
                        // Complete move
                        char old_dst_path[PATH_MAX];
                        const char* rel = pending_move_src + strlen(source);
                        snprintf(old_dst_path, PATH_MAX, "%s%s", target, rel);

                        rename(old_dst_path, event_dst_path);

                        if (event->mask & IN_ISDIR)
                            update_watch_paths(&map, pending_move_src, event_src_path);

                        pending_cookie = 0;
                        pending_move_src[0] = '\0';
                    }
                    else
                    {
                        // Moved from outside; same as create
                        if (event->mask & IN_ISDIR)
                        {
                            mkdir(event_dst_path, 0755);
                            add_watch_recursive_monitor(notify_fd, &map, event_src_path);
                            copy_recursive(event_src_path, event_dst_path, source, target);
                        }
                        else
                            copy_item(event_src_path, event_dst_path, source, target);
                    }
                }
            }

            i += sizeof(struct inotify_event) + event->len;
        }
    }

    close(notify_fd);
}
