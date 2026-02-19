#include "../include/functions.h"

//
void end_manage(char** args, children_data_t* data, int argCount)
{
    if (strcmp(args[0], "end") != 0)
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

    char cwd[PATH_MAX];
    if (getcwd(cwd, PATH_MAX) == NULL)
        ERR("getcwd");

    // Loop through all target paths
    for (int i = 2; i < argCount; i++)
    {
        char* target_raw = args[i];
        char target_abs_est[PATH_MAX];

        // Reconstruct target path as in add.c
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
        int found = 0;
        // Find the child handling this backup
        for (int j = 0; j < data->count; j++)
        {
            if (strcmp(data->children[j].source_path, source_abs) == 0 &&
                strcmp(data->children[j].target_path, target_abs_est) == 0)
            {
                printf("Ending backup from %s to %s\n", source_abs, target_abs_est);

                if (kill(data->children[j].pid, SIGTERM) != 0)
                    ERR("kill");

                // Prevent zombies
                waitpid(data->children[j].pid, NULL, 0);

                free(data->children[j].source_path);
                free(data->children[j].target_path);
                // Swap current spot with the last element
                data->children[j] = data->children[data->count - 1];
                data->count--;

                if (data->count > 0)
                {
                    data->children = realloc(data->children, data->count * sizeof(child_info_t));
                    if (data->children == NULL)
                        ERR("realloc");
                }
                else
                {
                    free(data->children);
                    data->children = NULL;
                }

                found = 1;
                break;
            }
        }

        if (!found)
            printf("Error No active backup found for %s -> %s\n", source_abs, target_abs_est);
    }
}
