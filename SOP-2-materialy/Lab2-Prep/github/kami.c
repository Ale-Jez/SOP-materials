#define _GNU_SOURCE
#include <asm-generic/errno.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_MSG_COUNT 4
#define MAX_ITEMS 8
#define MIN_ITEMS 2
#define SHOP_QUEUE_NAME "/shop"
#define MSG_SIZE 128
#define TIMEOUT 2
#define CLIENT_COUNT 8
#define OPEN_FOR 8
#define START_TIME 8
#define MAX_AMOUNT 16

static const char* const UNITS[] = {"kg", "l", "dkg", "g"};
static const char* const PRODUCTS[] = {"mięsa", "śledzi", "octu", "wódki stołowej", "żelatyny"};

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))


void ms_sleep(long milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;                  
    ts.tv_nsec = (milliseconds % 1000) * 1000000L;    

   
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {  }
}

typedef struct {
    mqd_t mqd;
} info;

void sethandler(void (*f)(int, siginfo_t *, void *), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = f;
    act.sa_flags = SA_SIGINFO;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void receivehandler(int sig,siginfo_t *si,void *ucontext){

    info *infoptr  = (info*)si->si_value.sival_ptr;
    
    mqd_t shopqueue = infoptr ->mqd;

    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = infoptr ;

    if (mq_notify(shopqueue, &sev) == -1) {
        if (errno != EBUSY && errno != EBADF)
            ERR("mq_notify");
        return;
    }


    char u[30];
    char p[30];
    int a;
    unsigned int P;


        char msg[MSG_SIZE];

        while(1)
        {
            if(TEMP_FAILURE_RETRY(mq_receive(shopqueue,msg,MSG_SIZE,&P))<0)
            {
                if(errno == EAGAIN)
                {
                    break;

                }
                else
                {
                    ERR("mq receive");
                }

            }

            sscanf(msg,"%s %d %s",u,&a,p);

            if(P!=0)
            {
                printf("Please go to the end of the line!\n");
            }
            else
            {
                switch(rand()%3)
                {
                    case 0:
                        printf("come back tmrw\n");
                        break;
                    case 1:
                        printf("out of stock\n");
                        break;
                    case 2:
                        printf("There is an item in the packing zone that shouldn’t be there.\n");
                        break;

                }

            }



            ms_sleep(100);
        }

    

}


void clientwork(mqd_t shopqueue)
{

    shopqueue = TEMP_FAILURE_RETRY(mq_open(SHOP_QUEUE_NAME,O_WRONLY));

    if(shopqueue == (mqd_t)-1){ERR("mq open");}

    srand(getpid());

    int n = (MIN_ITEMS + rand())%MAX_ITEMS;

    char u[30];
    char p[30];
    int a,P;

    for(int i=0;i<n;i++)
    {

        strcpy(u,UNITS[rand()%4]);
        strcpy(p,PRODUCTS[rand()%5]);

        a = 1 + rand()%MAX_AMOUNT;
        P = rand()%2;

        char msg[MSG_SIZE];

        snprintf(msg,MSG_SIZE,"%s %d %s",u,a,p);

        struct timespec ts;
        ts.tv_sec = 1;   
        ts.tv_nsec = 0;

        if(TEMP_FAILURE_RETRY(mq_timedsend(shopqueue,msg,MSG_SIZE,P,&ts))<0){
          
            if(errno == ETIMEDOUT)
            {
                printf("I will never come here..\n");
            
                errno = 0;
                break;
            }
            else
            {
                ERR("mq timed send");


            }
          
        }

      // printf("successfully added item!\n");




    }

   

    if(mq_close(shopqueue)){ERR("mq close");}

}

void selfcheckoutwork(mqd_t shopqueue)
{

    shopqueue = TEMP_FAILURE_RETRY(mq_open(SHOP_QUEUE_NAME,O_NONBLOCK |O_RDONLY));

    if(shopqueue == (mqd_t)-1){ERR("mq open");}

    srand(getpid());

    int probability = rand()%4;

    if(probability == 0)
    {
        printf("closed today..\n");
        if(mq_close(shopqueue)){ERR("mq close");}
        return;

    }
    else
    {

        printf("open today..\n");

        sethandler(receivehandler,SIGRTMIN);

        info *infoptr  = malloc(sizeof(info));

        if(!infoptr ){ERR("malloc");}

        infoptr ->mqd = shopqueue;

        struct sigevent sev;
        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = SIGRTMIN;
        sev.sigev_value.sival_ptr = infoptr ;

        if (mq_notify(shopqueue, &sev) == -1) {ERR("mq_notify");}

        for (int i = 0; i < OPEN_FOR; i++) {
            int h = START_TIME + i;
            printf("hour: %d:00\n", h);
            ms_sleep(200);
        }


    }


    if(mq_close(shopqueue)){ERR("mq close");}


    printf("Store closing.\n");
    sleep(1);

}



int main(void)
{

    if (mq_unlink(SHOP_QUEUE_NAME) == -1 && errno != ENOENT) {
        ERR("mq_unlink");
    }

    struct mq_attr attr = {};
    attr.mq_maxmsg = MAX_MSG_COUNT;
    attr.mq_msgsize =  MSG_SIZE;

    mqd_t shopqueue = TEMP_FAILURE_RETRY(mq_open(SHOP_QUEUE_NAME,O_CREAT,0666,&attr));

    if(shopqueue == (mqd_t)-1)
    {
        ERR("mq open");
    }

    if(mq_close(shopqueue)){ERR("mq close");}


    
    //self checkout

    switch(fork())
    {
        case 0:

            selfcheckoutwork(shopqueue);

            exit(EXIT_SUCCESS);
            break;
        case -1:
            ERR("fork");
    }

    ms_sleep(200);


    //clients

    for(int i=0;i<CLIENT_COUNT;i++)
    {
        switch(fork())
        {
            case 0:

                clientwork(shopqueue);


                exit(EXIT_SUCCESS);
                break;
            case -1:
                ERR("fork");
                break;
        }
    }



    

    while(wait(NULL)>0){}

    printf("End...\n");

    return EXIT_SUCCESS;
}