#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define UNUSED(x) (void)(x)

void usage(const char *pname)
{
    fprintf(stderr, "USAGE: %s p1 p2 ... pn\n", pname);  // TODO
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    UNUSED(argc);
    usage(argv[0]);

    exit(EXIT_SUCCESS);
}
