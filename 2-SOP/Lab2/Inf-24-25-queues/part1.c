#include <asm-generic/errno.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
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

typedef struct timespec timespec_t;

static const char* const UNITS[] = {"kg", "l", "dkg", "g"};
static const char* const PRODUCTS[] = {"mięsa", "śledzi", "octu", "wódki stołowej", "żelatyny"};

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

void msleep(unsigned int milisec)
{
    time_t sec = (int)(milisec / 1000);
    milisec = milisec - (sec * 1000);
    timespec_t req = {0};
    req.tv_sec = sec;
    req.tv_nsec = milisec * 1000000L;
    if (nanosleep(&req, &req))
        ERR("nanosleep");
}

void spawn_children()
{
    int res;

    for (int i = 0; i < CLIENT_COUNT; i++)
    {
        if (-1 == (res = fork()))
            ERR("fork");

        if (res == 0 && i > 0)  // Case child (client)
        {
            printf("[%d] I will never come here…\n", getpid());
            exit(EXIT_SUCCESS);
        }

        if (res == 0 && i == 0)  // Case child (self checkout)
        {
            msleep(200);
            printf("Store closing.\n");
            exit(EXIT_SUCCESS);
        }

        if (res > 0 && i == 0)
        {
            msleep(200);
        }
    }
}

int main(void)
{
    mq_unlink(SHOP_QUEUE_NAME);
    mqd_t shop_queue_fd;
    struct mq_attr attributes = {};
    attributes.mq_maxmsg = MAX_MSG_COUNT;
    attributes.mq_msgsize = MSG_SIZE;
    if (-1 == (shop_queue_fd = mq_open(SHOP_QUEUE_NAME, O_CREAT | O_EXCL | O_RDWR, 0600, attributes)))
        ERR("mq_open");

    mq_close(shop_queue_fd);

    spawn_children();

    while (wait(NULL) > 0)
        ;

    printf("END\n");
    mq_unlink(SHOP_QUEUE_NAME);
    return EXIT_SUCCESS;
}
