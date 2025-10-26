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
#include <limits.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define MAX_GROUPS 20
#define MAX_STUDENTS 256

int groups_count[MAX_GROUPS + 1];

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s path\n", name);
    exit(EXIT_FAILURE);
}

int walk1 (const char* path, const struct stat* s, int type, struct FTW* f)
{
    int N;
    int inGroup;

    
    char filename[PATH_MAX];
    strcpy(filename, path + f->base); //get just the game


    // Skipping files not starting with "grupa"
    if(strncmp(filename, "grupa", 5) != 0)
        return 0;
    
    // Skipping links
    if (type == FTW_SL)
    {
        fprintf(stderr, "%s: Link Detected!\n", filename);
        return 0;
    }   

    // Skipping non-regular files
    if (!S_ISREG(s->st_mode))
        return 0;
    
    // Everything has been checked, we process the file
    N = atoi(filename + 5); // Skip first 5 characters ("grupa"), crazy pointer arithmetic

    FILE* file;
    if ((file = (fopen(path, "r"))) == NULL)
        ERR("fopen");

    fscanf(file, "%d", &inGroup); // this cant fail since "Możesz założyć, że dane są poprawne"

    printf("Grupa %d ma %d studentów\n", N, inGroup);

    if (fclose(file))
        ERR("fclose");

    return 0;
}

void get_groups_count(const char *dirpath, int counts[MAX_GROUPS + 1])
{
    if (nftw(dirpath, walk1, 1, FTW_PHYS)) //nftw depth 1 because it's easier
        ERR("nftw");
}

void process_file(const char* filepath, int group_count) { printf("Przetworzenie pliku z ocenami (TODO)\n"); }

void batch_process(const char* dirpath) { printf("Przetworzenie ocen w drzewie katalogow (TODO)\n"); }

int main(int argc, char** argv) 
    {
    
    //accepting only 2 parameters
    if (argc != 2)
        usage(argv[0]);
    
    // we check if the second argument is a valid file
    struct stat filestat;
    const char* path = argv[1];
    if (lstat (path, &filestat))
        usage(argv[0]);

    //reading the input
    char word[10]; //word buffer
    fgets(word, 10, stdin);
    int len = strlen(word);
    //since we enter a newline
    word[len - 1] = '\0'; //we can also skip this and add a newline to strcmp later

    // variables for the purpose of calling the functions
    int counts[MAX_GROUPS + 1];
    int groupcount;

    if (strcmp("groups", word) == 0)
    {
        // jeśli argument jest katalogiem, to wywołuje funkcję get_groups_count;
        // w przeciwnym przypadku zgłasza błąd i kończy działanie
        if (S_ISDIR(filestat.st_mode)) 
            get_groups_count(path, counts); 
        else 
        {
            fputs("Must be a directory\n", stderr);
            exit(EXIT_FAILURE);
        }
    }

    else if (strcmp("process", word) == 0)
    {
        // jeśli argument jest plikiem, to prosi o podanie liczby grup w pliku, a następnie wywołuje funkcję process_file;
        // w przeciwnym przypadku zgłasza błąd i kończy działanie
        if (S_ISREG(filestat.st_mode))
        {
            fputs("Enter the number of groups in the file: ", stdout);
            if (scanf("%d", &groupcount) == 0)
                {
                    ERR("scanf, not a number");
                }
            process_file(path, groupcount);
        }
        else
        {
            fputs("Must be a regular file\n", stderr);
            exit(EXIT_FAILURE);
        }
    }

    else if (strcmp("batch", word) == 0)
    {
        //jeśli argument jest katalogiem, to wywołuje funkcję batch_process;
        // w przeciwnym przypadku zgłasza błąd i kończy działanie

        if (S_ISDIR(filestat.st_mode))
        {
            batch_process(path);
        }
        else 
        {
            fputs("Must be a directory\n", stderr);
            exit(EXIT_FAILURE);
        }
    }

    // Podanie innego słowa powoduje wypisanie stosownego komunikatu i wyjście z programu.
    else
    {
        fputs("groups, process or batch expected\n", stderr);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;  
    }

// Pliki grupaN zawieraja jedna liczbe - liczbe studentow w grupie tekstowo

// Pliki z ocenami zMgN - tyle liczb calkowitych (punktow za zadanie) ile studentow w grupie binarnie

// Nie definiuj zmiennych globalnych, nie korzystaj jawnie z groups_count.