#define _XOPEN_SOURCE 500
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(const char* name)
{
    fprintf(stderr, "USAGE: %s command input\n", name);
    exit(EXIT_FAILURE);
}

int histogram[256];
int qsort_comparison_function(const void* left, const void* right) { return 0; }

void analyze_file(const char* input)
{
    FILE* file = fopen(input, "r");
    if (file == NULL)
    {
        ERR("Couldn't open a file");
        return;
    }

    char c;
    while ((c = fgetc(file)) != EOF)
    {
        histogram[(int)c]++;
    }

    for (int i = 0; i < 256; i++)
    {
        if (histogram[i] != 0)
        {
            printf("%d %c\n", histogram[i], 'l');
        }
    }

    // printf("Convert agent's message to intermediate cypher (TODO).\n");
}

void decode_message(const char* input) { printf("Decode message from intermediate cypher (TODO).\n"); }
void batch_decode(const char* input) { printf("Convert and decode the entire directory tree (TODO).\n"); }

int main(int argc, char** argv)
{
    if (argc != 3)
        usage(argv[0]);
    char* cmd = argv[1];
    char* path = argv[2];
    struct stat sb;
    if (lstat(path, &sb) != 0)
    {
        ERR("Couldn't check for file status\n");
        return EXIT_FAILURE;
    }

    if (strcmp(cmd, "analyze") == 0)
    {
        if (S_ISREG(sb.st_mode) || S_ISLNK(sb.st_mode))
        {
            analyze_file(path);
            return EXIT_SUCCESS;
        }
        else
        {
            ERR("Not a regular file or a link\n");
            return EXIT_FAILURE;
        }
    }

    if (strcmp(cmd, "decode") == 0)
    {
        if (S_ISREG(sb.st_mode))
        {
            decode_message(path);
            return EXIT_SUCCESS;
        }
        else
        {
            ERR("Not a regular file\n");
            return EXIT_FAILURE;
        }
    }

    if (strcmp(argv[1], "batch") == 0)
    {
        if (S_ISDIR(sb.st_mode))
        {
            batch_decode(path);
            return EXIT_SUCCESS;
        }

        else
        {
            ERR("Not a directory\n");




            
            return EXIT_FAILURE;
        }
    }

    else
    {
        ERR("Inappropriate command passed\n");
        usage(argv[1]);
        return EXIT_FAILURE;
    }
}
