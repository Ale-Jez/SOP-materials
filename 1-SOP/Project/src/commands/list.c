#include "../include/common.h"

//
void list_manage(char** args, children_data_t* data)
{
    if (strcmp(args[0], "list") != 0)
        return;

    if (data->count <= 0)
    {
        printf("No folders are backuped\n");
        return;
    }

    printf("Listing backuped folders\n");
    printf("ID | source | \t destination\n");
    for (int i = 0; i < data->count; i++)
    {
        printf("%d. %s %s\n", i + 1, data->children[i].source_path, data->children[i].target_path);
    }
}
