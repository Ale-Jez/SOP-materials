#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)             \
    (__extension__({                               \
        long int __result;                         \
        do                                         \
            __result = (long int)(expression);     \
        while (__result == -1L && errno == EINTR); \
        __result;                                  \
    }))
#endif

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define UNUSED(x) (void)(x)

#define SLEIGHT_MAX_PAYLOAD (200)
#define ASSEMBLY_LINE_CAPACITY (8)
#define SLEIGHT_CAPACITY (100)

typedef enum
{
    Ordered,
    Assembled,
    Wrapped,
    Delivered
} GiftState;

typedef struct
{
    GiftState State;
    int Weight;
    int Id;
} Gift;

void random_sleep(int base, int add_time)
{
    struct timespec sleep_time = {0, (base + rand() % add_time) * 1000000L};
    TEMP_FAILURE_RETRY(nanosleep(&sleep_time, &sleep_time));
}

Gift *GetNextGiftOrder()
{
    static int gift_id = 0;
    random_sleep(2, 10);
    Gift *gift = malloc(sizeof(Gift));
    if (gift == NULL)
        ERR("malloc");
    *gift = (Gift){Ordered, rand() % 25 + 5, gift_id++};
    fprintf(stdout, "Gift %d was ordered!\n", gift->Id);
    return gift;
}

void AssembleGift(Gift *gift)
{
    if (gift->State > Ordered)
    {
        fprintf(stderr, "Gift %d was already assembled!\n", gift->Id);
        return;
    }
    random_sleep(5, 5 + gift->Weight);
    fprintf(stdout, "Gift %d was assembled!\n", gift->Id);
    gift->State = Assembled;
}

void WrapGift(Gift *gift)
{
    if (gift->State < Assembled)
    {
        fprintf(stderr, "Gift %d must be assembled first!\n", gift->Id);
        return;
    }
    if (gift->State > Assembled)
    {
        fprintf(stderr, "Gift %d was already wrapped!\n", gift->Id);
        return;
    }
    random_sleep(5, 5 + gift->Weight / 2);
    fprintf(stdout, "Gift %d was wrapped!\n", gift->Id);
    gift->State = Wrapped;
}

void MillGift(Gift *gift)
{
    fprintf(stdout, "Gift %d was milled!\n", gift->Id);
    free(gift);
}

void DeliverGifts(Gift **gifts, int count)
{
    int payload_size = 0;
    for (int i = 0; i < count; ++i)
    {
        Gift *gift = gifts[i];
        if (gift->State != Wrapped)
        {
            fprintf(stderr, "Gift %d was not wrapped for the delivery!\n", gift->Id);
            return;
        }
        payload_size += gifts[i]->Weight;
    }
    if (payload_size > SLEIGHT_MAX_PAYLOAD)
    {
        fprintf(stderr, "Sleight capacity exceeded! %d > %d\n", payload_size, SLEIGHT_MAX_PAYLOAD);
        return;
    }

    fprintf(stdout, "Santa goes out for delivery!\n");
    random_sleep(10, 50);
    for (int i = 0; i < count; ++i)
    {
        random_sleep(1, 5);
        gifts[i]->State = Delivered;
        fprintf(stdout, "Gift %d was delivered!\n", gifts[i]->Id);
        free(gifts[i]);
        gifts[i] = NULL;
    }
    random_sleep(10, 50);
    fprintf(stdout, "Santa came back!\n");
}