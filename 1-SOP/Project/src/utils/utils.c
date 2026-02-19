#include "../include/common.h"
#include "../include/functions.h"

// functions taken from regular laboraties' initial code
// they were extremely common during labs in this and previous year
// for example:
// https://pages.mini.pw.edu.pl/~hermant/en/courses/operating-systems-1/l2-2025/

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void ms_sleep(unsigned int milli)
{
    time_t sec = (int)(milli / 1000);
    milli = milli - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milli * 1000000L;
    if (TEMP_FAILURE_RETRY(nanosleep(&ts, &ts)))
        ERR("nanosleep");
}
//

//------------------------------------
// my own functions
//------------------------------------

//
int is_subpath(const char* parent, const char* child)
{
    size_t len_p = strlen(parent);
    size_t len_c = strlen(child);
    if (len_c < len_p)
        return 0;
    if (strncmp(parent, child, len_p) == 0)
    {
        if (child[len_p] == '\0' || child[len_p] == '/')
            return 1;
    }
    return 0;
}

int is_dir_empty(const char* path)
{
    DIR* dir = opendir(path);
    if (dir == NULL)
        ERR("opendir");
    struct dirent* d;
    int count = 0;
    while ((d = readdir(dir)) != NULL)
    {
        if (strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0)
        {
            count++;
            break;
        }
    }
    closedir(dir);
    if (count == 0)
        return 1;
    else
        return 0;
}

// Copies a single item or creates a directory (NO recursion)
void copy_item(const char* src_path, const char* dst_path, const char* root_src, const char* root_dst)
{
    struct stat st;
    if (lstat(src_path, &st) == -1)
        ERR("lstat");

    if (S_ISDIR(st.st_mode))
        mkdir(dst_path, st.st_mode);

    else if (S_ISLNK(st.st_mode))
    {
        char link_target[PATH_MAX];
        ssize_t len = readlink(src_path, link_target, sizeof(link_target) - 1);
        if (len != -1)
        {
            link_target[len] = '\0';
            if (link_target[0] == '/' && strncmp(link_target, root_src, strlen(root_src)) == 0)
            {
                char new_link[PATH_MAX];
                snprintf(new_link, PATH_MAX, "%s%s", root_dst, link_target + strlen(root_src));
                unlink(dst_path);
                symlink(new_link, dst_path);
            }
            else
            {
                unlink(dst_path);
                symlink(link_target, dst_path);
            }
        }
    }
    else if (S_ISREG(st.st_mode))
    {
        int f_src = open(src_path, O_RDONLY);
        int f_dst = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
        if (f_src != -1 && f_dst != -1)
        {
            char buf[MAX_BUF];
            ssize_t bytes;
            while ((bytes = read(f_src, buf, sizeof(buf))) > 0)
                write(f_dst, buf, bytes);
        }
        if (f_src != -1)
            close(f_src);
        if (f_dst != -1)
            close(f_dst);
    }
}

void remove_recursive(const char* path)
{
    if (remove(path) == 0)
        return;

    DIR* d = opendir(path);
    if (d == NULL)
        ERR("opendir");

    struct dirent* p;
    while ((p = readdir(d)))
    {
        if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
            continue;
        char buf[PATH_MAX];
        snprintf(buf, PATH_MAX, "%s/%s", path, p->d_name);
        struct stat st;
        if (lstat(buf, &st) == 0 && S_ISDIR(st.st_mode))
            remove_recursive(buf);
        else
            unlink(buf);
    }
    closedir(d);
    rmdir(path);
}

void copy_recursive(const char* src_base, const char* dst_base, const char* root_src, const char* root_dst)
{
    DIR* dir = opendir(src_base);
    if (dir == NULL)
        ERR("opendir");

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char src_path[PATH_MAX];
        char dst_path[PATH_MAX];
        snprintf(src_path, PATH_MAX, "%s/%s", src_base, entry->d_name);
        snprintf(dst_path, PATH_MAX, "%s/%s", dst_base, entry->d_name);

        struct stat st;
        if (lstat(src_path, &st) == -1)
            continue;

        if (S_ISDIR(st.st_mode))
        {
            mkdir(dst_path, st.st_mode);
            copy_recursive(src_path, dst_path, root_src, root_dst);
        }
        else
            copy_item(src_path, dst_path, root_src, root_dst);
    }
    closedir(dir);
}
