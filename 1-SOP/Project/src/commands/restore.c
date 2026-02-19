#include "../include/common.h"
#include "../include/functions.h"

//

void perform_restore(const char* current_source, const char* current_target, const char* root_source,
                     const char* root_target)
{
    DIR* dir_s = opendir(current_source);
    if (dir_s == NULL)
        ERR("opendir");
    struct dirent* d;
    struct stat st_s, st_t;
    char path_s[PATH_MAX];
    char path_t[PATH_MAX];

    // ---------------------------------------------------------
    // Delete files which aren't in target (backup)
    // ---------------------------------------------------------
    while ((d = readdir(dir_s)) != NULL)
    {
        if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
            continue;

        snprintf(path_s, PATH_MAX, "%s/%s", current_source, d->d_name);
        snprintf(path_t, PATH_MAX, "%s/%s", current_target, d->d_name);

        if (lstat(path_t, &st_t) == -1)
        {
            remove_recursive(path_s);
            continue;
        }

        if (lstat(path_s, &st_s) == 0)
        {
            if ((S_ISDIR(st_s.st_mode) && !S_ISDIR(st_t.st_mode)) || (!S_ISDIR(st_s.st_mode) && S_ISDIR(st_t.st_mode)))
            {
                remove_recursive(path_s);
            }
        }
    }
    closedir(dir_s);

    // ---------------------------------------------------------
    // Copy from target (backup) to source
    // ---------------------------------------------------------
    DIR* dir_t = opendir(current_target);
    if (dir_t == NULL)
        ERR("opendir");

    while ((d = readdir(dir_t)) != NULL)
    {
        if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
            continue;

        snprintf(path_s, PATH_MAX, "%s/%s", current_source, d->d_name);
        snprintf(path_t, PATH_MAX, "%s/%s", current_target, d->d_name);

        if (lstat(path_t, &st_t) == -1)
            continue;

        int source_exists = (lstat(path_s, &st_s) == 0);

        if (S_ISDIR(st_t.st_mode))
        {
            if (source_exists == 0)
                mkdir(path_s, st_t.st_mode);

            // Further recursion
            perform_restore(path_s, path_t, root_source, root_target);
        }
        else  // file or symlink
        {
            int needs_copy = 0;
            if (source_exists == 0)
                needs_copy = 1;
            else
            {
                if (st_s.st_size != st_t.st_size || st_s.st_mtime != st_t.st_mtime)
                    needs_copy = 1;
            }

            if (needs_copy)
            {
                // copy_item handles symlink adjustment
                // src_path = path in backup
                // dst_path = path in source (workspace)
                // root_src = Root of backup
                // root_dst = Root of source
                copy_item(path_t, path_s, root_target, root_source);
            }
        }
    }
    closedir(dir_t);
}

//
//
static void restore_command(char** args)
{
    // args[1] source, where we restore TO
    // args[2] target, where the backup is stored
    char* source_root = args[1];
    char* target_root = args[2];

    struct stat st;
    if (stat(source_root, &st) == -1)
        mkdir(source_root, 0755);

    perform_restore(source_root, target_root, source_root, target_root);
    return;
}

//
void restore_manage(char** args, children_data_t* data, int argCount)
{
    if (strcmp(args[0], "restore") != 0)
        return;
    if (argCount < 3)
    {
        printf("Error missing paths\n");
        return;
    }
    if (argCount > 3)
    {
        printf("Error too many arguments\n");
        return;
    }

    char* source_raw = args[1];
    char source_abs[PATH_MAX];
    if (!realpath(source_raw, source_abs))
    {
        if (errno == ENOENT)
        {
            if (source_raw[0] != '/')
            {
                getcwd(source_abs, PATH_MAX);
                strcat(source_abs, "/");
                strcat(source_abs, source_raw);
            }
            else
                strcpy(source_abs, source_raw);
        }
        else
        {
            printf("Error resolving source path\n");
            return;
        }
    }

    char* target_raw = args[2];
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
        printf("Error Target path is too long (truncated)\n");

    // Resolve target path (backup MUST exist to restore from it)
    char target_real[PATH_MAX];
    if (!realpath(target_abs_est, target_real))
    {
        printf("Error Backup directory does not exist: %s\n", target_abs_est);
        return;
    }

    if (is_subpath(source_abs, target_abs_est) || is_subpath(target_abs_est, source_abs))
        printf("Error Cannot restore backup into itself or parent (%s -> %s)\n", source_abs, target_abs_est);

    //
    // ---------------------------------------------------------
    // restore from backup
    printf("Restoring backup from %s to %s\n", args[1], args[2]);

    pid_t pid = fork();
    if (pid == -1)
        ERR("fork");
    else if (pid == 0)  // Child
    {
        char* child_args[] = {"restore", source_abs, target_abs_est};
        restore_command(child_args);
        exit(EXIT_SUCCESS);
    }
    else  // Parent
    {
        int status;
        waitpid(pid, &status, 0);
        printf("Restore complete.\n");
    }
}
