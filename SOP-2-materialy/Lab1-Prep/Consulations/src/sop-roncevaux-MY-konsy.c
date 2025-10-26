
#include <bits/types/siginfo_t.h>
#include <signal.h>
#include <threads.h>
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

typedef struct knight_t {
    char name[MAX_KNIGHT_NAME_LENGTH];
    int hp;
    int strength;

}knight_t;

int count_descriptors();
void msleep(int millisec);


void child_work(knight_t knight, int readEnd, int* enemyPipes, int enemyNum)
{
    //for random damage
    srand(getpid());

    
    //copy that function usage fromstack Oberflow
    //set readEnd to nonBlocking mode
    fcntl(readEnd, F_SETFL, O_NONBLOCK);
    int res;
    char result;
    int alive = 1;
    int aliveEnemies = enemyNum;

    
    while(alive && aliveEnemies > 0){
        //loop for taking damage
        while(alive && aliveEnemies > 0)
        {
            res = read(readEnd, &result, 1);
            
            //end of transmission
            if (res == 0){
                aliveEnemies = 0;
                break; //all the writers have ended
            }
            if (res == -1 && errno == EAGAIN)
            {
                break;
            }
            else if (res == -1)
                ERR("read");
            if (res == 0) //we take damage
            {
                knight.hp -= result;
                if (knight.hp < 0)
                {
                    printf("%s dies\n", knight.name);
                    alive = 0;
                }
            }

        }


        //loop for dealing damage
        while (aliveEnemies > 0 && alive)
        {
            int enemy = rand() % aliveEnemies;
        //damage is char because we send a byte
            char damage = rand() % knight.strength;

            int res = write(enemyPipes[2*enemy+1], &damage, 1);
            if (res == -1){
                if (errno == EPIPE)
                {
                    aliveEnemies--;
                    int temp = enemyPipes[2* enemy +1];
                    enemyPipes[2* enemy +1] = enemyPipes[2*aliveEnemies + 1];
                    enemyPipes[2*aliveEnemies + 1] = temp;
                    continue;
                }
                else
                    ERR("write");
            }


            if (damage == 0)
                printf("%s, attacks his enemy, however he deflected\n", knight.name);
            if (damage > 1 && damage <= 5)
                printf("%s, goes to strike, he hit right and well\n", knight.name);
            if (damage >= 6)
                printf("%s, strikes a powerful blow, the shield he breaks and inflicts a big wound\n", knight.name);
            msleep(rand() % 10 + 1);
            //breaking after each attack
            break;        
        }
    }



    close (readEnd);
    for (int i= 0; i < enemyNum; i++)
    {
        close(enemyPipes[2*i+1]); //we close enemy write ends
    }
    printf("Opened descriptors: %d\n", count_descriptors());
}


void create_pipes_and_fork(knight_t* franci, int franciNo, knight_t* saraceni, int saraceniNo)
{
    int* pipeSaraceni;
    int* pipeFranci;
    if ((pipeSaraceni = calloc(saraceniNo*2, sizeof(int))) == NULL)
        ERR("calloc");
    if ((pipeFranci = calloc(franciNo*2, sizeof(int))) == NULL)
        ERR("calloc");

    for (int i = 0; i < franciNo; i++)
    {
        if (pipe(&pipeFranci[2*i]) == -1)
            ERR("pipe");
    }
    for (int i = 0; i < saraceniNo; i++)
    {
        if (pipe(&pipeSaraceni[2*i]) == -1)
            ERR("pipe");
    }

    for (int i = 0; i < franciNo; i++)
    {
        int res = fork();
        if (res == -1)
            ERR("fork");
        if (res == 0) {
            for (int j = 0; j < franciNo; j++)
            {
                if (i != j){
                    close(pipeFranci[2*j]); //closing the reading end  (except our own)
                }
                close(pipeFranci[2*j+1]); //close the writing end
            }


            for (int j = 0; j < saraceniNo; j++)
            {
                close(pipeSaraceni[2*j]); //closing the reading end for saraceni
                
            }

            printf("I am Frankish knight %s. I will serve my king with my %d HP, and %d attack.\n", franci[i].name, franci[i].hp, franci[i].strength);
            child_work(franci[i], pipeFranci[2*i], pipeSaraceni, saraceniNo);


            free(franci);
            free(saraceni);
            free(pipeFranci);
            free(pipeSaraceni);  
            exit(EXIT_SUCCESS);
        }

    }


    for (int i = 0; i < saraceniNo; i++)
    {
        int res = fork();
        if (res == -1)
            ERR("fork");
        if (res == 0) {
            for (int j = 0; j < saraceniNo; j++)
            {
                if (i != j){
                    close(pipeSaraceni[2*j]); //closing the reading end  (except our own)
                }
                close(pipeSaraceni[2*j+1]); //close the writing end
            }


            for (int j = 0; j < franciNo; j++)
            {
                close(pipeFranci[2*j]); //closing the reading end for saraceni
                
            }

            printf("I am Spanish knight %s. I will serve my king with my %d HP, and %d attack.\n", saraceni[i].name, saraceni[i].hp, saraceni[i].strength);
            child_work(saraceni[i], pipeSaraceni[2*i],pipeFranci, franciNo);


            free(franci);
            free(saraceni);
            free(pipeFranci);
            free(pipeSaraceni);  
            exit(EXIT_SUCCESS);
        }

    }
   
    

    for (int i = 0; i < 2*franciNo; i++)
    {
        close(pipeFranci[i]);
    }
    for (int i = 0; i < 2*saraceniNo; i++)
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

    set_handler(SIG_IGN, SIGPIPE);



    franci_f = fopen("franci.txt", "r");
    if (franci_f == NULL)
        ERR("Franci fopen");
    saraceni_f = fopen("franci.txt", "r");
    if (saraceni_f == NULL)
        ERR("Saraceni fopen");

    int franciNo;
    fscanf(franci_f, "%d", &franciNo);
    int saraceniNo;
    fscanf(saraceni_f, "%d", &saraceniNo);

    knight_t* franci = calloc(franciNo, sizeof(knight_t));
    knight_t* saraceni = calloc(saraceniNo, sizeof(knight_t));
    if (franci == NULL)
        ERR("Franci calloc");
    if (saraceni == NULL)
        ERR("Sarceni calloc");

    for (int i = 0; i < franciNo; i++)
    {
        fscanf(franci_f, "%s %d %d", franci[i].name, &franci[i].hp, &franci[i].strength);
    }


    for (int i = 0; i < saraceniNo; i++)
    {
        fscanf(saraceni_f, "%s %d %d", saraceni[i].name, &saraceni[i].hp, &saraceni[i].strength);
    }

//satge2
    create_pipes_and_fork(franci, franciNo, saraceni, saraceniNo);

//parent
    while(wait(NULL) > 0);


    free(franci);
    free(saraceni);



    srand(time(NULL));
    printf("Opened descriptors: %d\n", count_descriptors());

    return EXIT_SUCCESS;
}
