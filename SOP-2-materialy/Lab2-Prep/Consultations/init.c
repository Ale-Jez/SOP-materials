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

static const char* const UNITS[] = {"kg", "l", "dkg", "g"};
static const char* const PRODUCTS[] = {"mięsa", "śledzi", "octu", "wódki stołowej", "żelatyny"};

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))


int main(void)
{
    return EXIT_SUCCESS;
}
