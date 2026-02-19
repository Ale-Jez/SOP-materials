#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define MAX_KNIGHT_NAME_LENGTH 20

typedef struct knight_t
{
    char name[MAX_KNIGHT_NAME_LENGTH];
    int HP;
    int attack;
} knight_t;

int count_descriptors();

void msleep(int millisec);

void child_work(knight_t knight, int readEnd, int* enemyPipes, int enemyNo)
{
    // For random damage
    srand(getpid());

    // Set readEnd to nonblocking mode
//Normally, if a process calls read() on a pipe that has no data available, it will block (i.e., wait) until data shows up.
//Non-blocking mode prevents that waiting;
    fcntl(readEnd, F_SETFL, O_NONBLOCK);
    int res;
    char buf;

    while (1)
    {
        // Loop for taking damage
        while (1)
        {
            res = read(readEnd, &buf, 1);

            // End if transmission
            if (res == 0)
                break;
            // We check for EAGAIN because of the nonblocking mode
            if (res == -1 && errno == EAGAIN)
            {
                // There's nothing to read, we want to start dealing damage
                break;
            }
            else if (res == -1)
                ERR("read");
            if (res == 1)
            {
                // We take damage
                knight.HP -= buf;
            }
        }

        // Loop for dealing damage
        while (1)
        {
            int enemy = rand() % enemyNo;
            char damage = rand() % knight.attack;  // Char because we want to send one byte

            int res;
            if ((res = write(enemyPipes[2 * enemy + 1], &damage, 1)) == -1)
                ERR("write");

            if (damage == 0)
                printf("%s, attacks his enemy, however he deflected\n", knight.name);
            if (damage >= 1 && damage <= 5)
                printf("%s, goes to strike, he hit right and well\n", knight.name);
            if (damage >= 6)
                printf("%s, strikes a powerful blow, the shield he breaks and inflicts a big wound\n", knight.name);

            msleep(rand() % 10 + 1);
        }
    }

    // Closing the pipes
    close(readEnd);
    for (int i = 0; i < enemyNo; i++)
    {
        close(enemyPipes[2 * i + 1]);
    }
}

void create_pipes_and_fork(knight_t* franci_k, int franciNo, knight_t* saraceni_k, int saraceniNo)
{
    int* pipeSaraceni;
    int* pipeFranci;
    if ((pipeSaraceni = calloc(saraceniNo * 2, sizeof(int))) == NULL)
        ERR("calloc");
    if ((pipeFranci = calloc(franciNo * 2, sizeof(int))) == NULL)
        ERR("calloc");

    for (int i = 0; i < franciNo; i++)
    {
        if (pipe(&pipeFranci[2 * i]) == -1)
            ERR("pipe");
    }

    for (int i = 0; i < saraceniNo; i++)
    {
        if (pipe(&pipeSaraceni[2 * i]) == -1)
            ERR("pipe");
    }

    // Opening the pipes
    for (int i = 0; i < franciNo; i++)
    {
        int res = fork();
        if (res == 0)
        {
            for (int j = 0; j < franciNo; j++)
            {
                if (i != j)
                {
                    close(pipeFranci[2 * j]);  // Closing the reading end (except our own)
                }
                close(pipeFranci[2 * j + 1]);  // Closing the writing end
            }

            for (int j = 0; j < saraceniNo; j++)  // Closing reading end for saraceni
            {
                close(pipeSaraceni[2 * j]);
            }
            printf("I am Frankish knight %s,. I will serve my king with my %d HP and %d attack\n", franci_k[i].name,
                   franci_k[i].HP, franci_k[i].attack);
            child_work(franci_k[i], pipeFranci[2 * i], pipeSaraceni, saraceniNo);
            free(franci_k);
            free(saraceni_k);
            free(pipeFranci);
            free(pipeSaraceni);
            exit(EXIT_SUCCESS);
        }
        if (res == -1)
            ERR("fork");
    }

    // Now for saraceni
    for (int i = 0; i < saraceniNo; i++)
    {
        int res = fork();
        if (res == 0)
        {
            for (int j = 0; j < saraceniNo; j++)
            {
                if (i != j)
                {
                    close(pipeSaraceni[2 * j]);  // Closing the reading end (except our own)
                }
                close(pipeSaraceni[2 * j + 1]);  // Closing the writing end
            }

            for (int j = 0; j < franciNo; j++)  // Closing reading end for franci
            {
                close(pipeFranci[2 * j]);
            }

            printf("I am Spanish knight %s,. I will serve my king with my %d HP and %d attack\n", saraceni_k[i].name,
                   saraceni_k[i].HP, saraceni_k[i].attack);
            child_work(saraceni_k[i], pipeSaraceni[2 * i], pipeFranci, franciNo);
            free(franci_k);
            free(saraceni_k);
            free(pipeFranci);
            free(pipeSaraceni);
            exit(EXIT_SUCCESS);
        }
        if (res == -1)
            ERR("fork");
    }

    // Closing the pipes
    for (int i = 0; i < franciNo * 2; i++)  // Times 2
    {
        close(pipeFranci[i]);
    }

    for (int i = 0; i < saraceniNo * 2; i++)
    {
        close(pipeSaraceni[i]);
    }

    free(pipeFranci);
    free(pipeSaraceni);
}

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void msleep(int millisec)
{
    struct timespec tt;
    tt.tv_sec = millisec / 1000;
    tt.tv_nsec = (millisec % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
    }
}

int count_descriptors()
{
    int count = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat stats;
    if ((dir = opendir("/proc/self/fd")) == NULL)
        ERR("opendir");
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    chdir("/proc/self/fd");
    do
    {
        errno = 0;
        if ((entry = readdir(dir)) != NULL)
        {
            if (lstat(entry->d_name, &stats))
                ERR("lstat");
            if (!S_ISDIR(stats.st_mode))
                count++;
        }
    } while (entry != NULL);
    if (chdir(path))
        ERR("chdir");
    if (closedir(dir))
        ERR("closedir");
    return count - 1;  // one descriptor for open directory
}

int main(int argc, char* argv[])
{
    FILE* franci_f;
    FILE* saraceni_f;

    if ((franci_f = fopen("franci.txt", "r")) == NULL)
    {
        printf("Franks have not arrived on the battlefield");
        ERR("fopen");
    }
    if ((saraceni_f = fopen("saraceni.txt", "r")) == NULL)
    {
        printf("Saraceni have not arrived on the battlefield");
        ERR("fopen");
    }

    int franciNo;
    fscanf(franci_f, "%d", &franciNo);

    int saraceniNo;
    fscanf(saraceni_f, "%d", &saraceniNo);

    knight_t* franci;
    if ((franci = calloc(franciNo, sizeof(knight_t))) == NULL)
        ERR("calloc");
    knight_t* saraceni;
    if ((saraceni = calloc(saraceniNo, sizeof(knight_t))) == NULL)
        ERR("calloc");

    for (int i = 0; i < franciNo; i++)
    {
        fscanf(franci_f, "%s %d %d", franci[i].name, &franci[i].HP, &franci[i].attack);
    }

    for (int i = 0; i < saraceniNo; i++)
    {
        fscanf(saraceni_f, "%s %d %d", saraceni[i].name, &saraceni[i].HP, &saraceni[i].attack);
    }

    // Freeing stuff
    fclose(saraceni_f);
    fclose(franci_f);

    // Stage02
    create_pipes_and_fork(franci, franciNo, saraceni, saraceniNo);

    // Parent wait for children
    while (wait(NULL) > 0)
        ;

    // Freeing stuff again
    free(franci);
    free(saraceni);

    srand(time(NULL));
    printf("Opened descriptors: %d\n", count_descriptors());

    return EXIT_SUCCESS;
}
