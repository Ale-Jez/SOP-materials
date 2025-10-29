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

#define MAX_GROUPS 20

int groups_count[MAX_GROUPS + 1];

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s path\n", name);
    exit(EXIT_FAILURE);
}

void get_groups_count(const char* dirpath, int counts[MAX_GROUPS + 1]) { printf("Wypisanie grup (TODO)\n"); }

void process_file(const char* filepath, int group_count) { printf("Przetworzenie pliku z ocenami (TODO)\n"); }

void batch_process(const char* dirpath) { printf("Przetworzenie ocen w drzewie katalogow (TODO)\n"); }

int main(int argc, char** argv) { return EXIT_SUCCESS; }
