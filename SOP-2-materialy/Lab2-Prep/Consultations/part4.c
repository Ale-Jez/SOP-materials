#define _GNU_SOURCE
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
static const char* const PRODUCTS[] = {"meat", "herrings", "vinegar", "table vodka", "gelatin"};

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

void register_notification(mqd_t* data);

void handle_messages(union sigval data)
{
    mqd_t* fd = data.sival_ptr;
    register_notification(fd);

    for (;;)
    {
        char mes[MSG_SIZE];
        unsigned int P;

        if (-1 != mq_receive(*fd, mes, MSG_SIZE, &P))
        {
            // printf("%s? ", mes);
            if (P == 0)
            {
                switch (rand() % 3)
                {
                    case 0:
                        printf("Come back tomorrow.\n");

                        break;
                    case 1:
                        printf("Out of stock\n");
                        break;
                    case 2:
                        printf("There is an item in the packing zone that shouldn’t be there.\n");
                        break;
                    default:
                        break;
                }
            }
            else
            {
                printf("Please go to the end of the line!\n");
            }
            msleep(100);
        }
        else
        {
            // It is EBADF not EBADFD for some reason
            if (errno == EAGAIN || errno == EBADF)
            {
                break;
            }
            ERR("mq_receive");
        }
        // Technically this should be there but the output looks bad
        // msleep(100);
    }
}

void register_notification(mqd_t* data)
{
    struct sigevent notification = {};
    notification.sigev_value.sival_ptr = data;
    notification.sigev_notify = SIGEV_THREAD;
    notification.sigev_notify_function = handle_messages;

    int res = mq_notify(*data, &notification);

    // It is EBADF not EBADFD for some reason
    if (res == -1 && errno != EBADF)
        ERR("mq_notify");
}

void self_checkout_work()
{
    mqd_t qfd;
    if (-1 == (qfd = mq_open(SHOP_QUEUE_NAME, O_RDONLY | O_NONBLOCK)))
        ERR("mq_open");

    srand(getpid());
    if (rand() % 4 == 0)
    {
        printf("Closed today.\n");
        mq_close(qfd);
        exit(EXIT_SUCCESS);
    }

    printf("Open today.\n");

    union sigval val;
    val.sival_ptr = &qfd;
    handle_messages(val);

    for (int i = 0; i < OPEN_FOR; i++)
    {
        printf("%d:00\n", START_TIME + i);

        msleep(200);
    }

    mq_close(qfd);
    printf("Store closing.\n");
    // msleep(1000);
}

void client_work()
{
    mqd_t qfd;

    if (-1 == (qfd = mq_open(SHOP_QUEUE_NAME, O_WRONLY)))
        ERR("mq_open");

    srand(getpid());
    int n;
    n = rand() % (MAX_ITEMS - MIN_ITEMS + 1) + MIN_ITEMS;

    for (int i = 0; i < n; i++)
    {
        int u = rand() % sizeof(UNITS) / sizeof(char*);
        int p = rand() % sizeof(PRODUCTS) / sizeof(char*);
        int a = rand() % (MAX_AMOUNT) + 1;
        unsigned int P = rand() % 2;

        char mes[MSG_SIZE];
        snprintf(mes, MSG_SIZE, "%d%s %s", a, UNITS[u], PRODUCTS[p]);

        timespec_t time;
        clock_gettime(CLOCK_REALTIME, &time);
        time.tv_sec++;

        if (-1 == mq_timedsend(qfd, mes, MSG_SIZE, P, &time))
        {
            if (errno == ETIMEDOUT)
            {
                printf("[%d] I will never come here…\n", getpid());
                break;
            }
            ERR("mq_timedsend");
        }

        // To make the output better???
        // msleep(1000);
    }

    mq_close(qfd);
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
            client_work();
            exit(EXIT_SUCCESS);
        }

        if (res == 0 && i == 0)  // Case child (self checkout)
        {
            // msleep(200);
            self_checkout_work();
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
    // Remember about passing an adress to the struct
    if (-1 == (shop_queue_fd = mq_open(SHOP_QUEUE_NAME, O_CREAT | O_EXCL | O_RDWR, 0600, &attributes)))
        ERR("mq_open");

    mq_close(shop_queue_fd);

    spawn_children();

    while (wait(NULL) > 0)
        ;

    printf("END\n");
    mq_unlink(SHOP_QUEUE_NAME);
    return EXIT_SUCCESS;
}
