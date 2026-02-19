#include "include/common.h"
#include "include/functions.h"

//
volatile sig_atomic_t running = 1;
void sig_handler(int sig) { running = 0; }

//

void printCommands()
{
    printf("Available commands: \n");
    printf("\t add <source path> <target paths>\n");
    printf("\t end <source path> <target paths>\n");
    printf("\t list\n");
    printf("\t restore <source path> <target path>\n");
    printf("\t exit\n");
}

//

int parseCommand(char* input, char** args)
{
    int count = 0;
    char* ptr = input;
    input[strcspn(input, "\n")] = 0;

    while (*ptr)
    {
        while (isspace((unsigned char)*ptr))
            ptr++;
        if (*ptr == '\0')
            break;

        if (*ptr == '"' || *ptr == '\'')
        {
            char quote = *ptr++;
            args[count++] = ptr;
            char* end = strchr(ptr, quote);
            if (end)
            {
                *end = '\0';
                ptr = end + 1;
            }
            else
            {
                ptr += strlen(ptr);
            }
        }
        else
        {
            args[count++] = ptr;
            while (*ptr && !isspace((unsigned char)*ptr))
                ptr++;
            if (*ptr)
            {
                *ptr = '\0';
                ptr++;
            }
        }
        if (count >= MAX_ARGS - 1)
            break;
    }
    args[count] = NULL;
    return count;
}

//

int main(int argc, char* argv[])
{
    printCommands();

    char inputBuffer[MAX_INPUT_LEN];
    char* args[MAX_ARGS];

    children_data_t* data = calloc(1, sizeof(children_data_t));
    if (data == NULL)
        ERR("calloc");
    data->children = calloc(1, sizeof(child_info_t));
    if (data->children == NULL)
        ERR("calloc");
    data->count = 0;

    // managing signals
    sethandler(sig_handler, SIGTERM);
    sethandler(sig_handler, SIGINT);

    // Ignore other signals
    sethandler(SIG_IGN, SIGQUIT);
    sethandler(SIG_IGN, SIGTSTP);
    sethandler(SIG_IGN, SIGUSR1);
    sethandler(SIG_IGN, SIGUSR2);

    //
    while (running)
    {
        printf("> ");

        if (!fgets(inputBuffer, MAX_INPUT_LEN, stdin))
            break;

        int argCount = parseCommand(inputBuffer, args);
        if (argCount == 0)
            continue;

        if (exit_manage(args) == 1)
            break;

        add_manage(args, data, argCount);
        end_manage(args, data, argCount);
        list_manage(args, data);
        restore_manage(args, data, argCount);
    }

    // terminate children
    for (int i = 0; i < data->count; i++)
        kill(data->children[i].pid, SIGTERM);

    while (wait(NULL) > 0)
    {
    }

    // freeing after exiting
    for (int i = 0; i < data->count; i++)
    {
        free(data->children[i].source_path);
        free(data->children[i].target_path);
    }
    free(data->children);

    free(data);
    return 0;
}
