#include "../include/functions.h"

//

int exit_manage(char** args)
{
    if (strcmp(args[0], "exit") == 0)
        return 1;
    else
        return 0;
}
