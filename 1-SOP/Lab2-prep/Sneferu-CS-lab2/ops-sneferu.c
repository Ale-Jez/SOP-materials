#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source)                                                            \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__),             \
   kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t last_sig;

ssize_t bulk_read(int fd, char *buf, size_t count) {
  ssize_t c;
  ssize_t len = 0;
  do {
    c = TEMP_FAILURE_RETRY(read(fd, buf, count));
    if (c < 0)
      return c;
    if (c == 0)
      return len; // EOF
    buf += c;
    len += c;
    count -= c;
  } while (count > 0);
  return len;
}

ssize_t bulk_write(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}


void usage(int argc, char *argv[]) {
  printf("%s p k \n", argv[0]);
  printf("\tp - path to file with stones\n");
  printf("\t0 < k < 8 - number of child processes\n");
  exit(EXIT_FAILURE);
}


void sig_handler(int sig) { last_sig = sig; }

void sigchld_handler(int sig)
{
    pid_t pid;
    for (;;)
    {
        pid = waitpid(0, NULL, WNOHANG);
        if (pid == 0)
            return;
        if (pid <= 0)
        {
            if (errno == ECHILD)
                return;
            ERR("waitpid");
        }
    }
}

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

ssize_t readline(int fd, char* buf, int offset)
{
    int i = offset;
    char character;
    ssize_t len = 0;
    ssize_t c;

    while (1)
    {
        if ((c = (bulk_read(fd, &character, 1))) < 0)
            return c;

        if (c == 0)
            break;

        buf[i] = character;

        i++;
        len++;
        if (character == '\n')
            break;
    }

    return len;
}


void child_work(int idx,  sigset_t oldmask, char* chunk, int read, char* p){
    printf("Child's ID: %d\n", idx);
    while (last_sig != SIGUSR1)
    {
        sigsuspend(&oldmask);
    }

    int count = 0;
    char name[100];
    sprintf(name, "%s-%d", p, idx);
  
//creating a file  
    int fd;
    if ((fd = TEMP_FAILURE_RETRY(open(name, O_CREAT | O_WRONLY | O_TRUNC, 0644))) < 0)
        ERR("open");

    for (int i = 0; i < read; i++){
// SIGINT HANDLING for a child
        if (last_sig == SIGINT)
        {
            printf("CHILD [%d]: received SIGINT!\n", getpid());
            kill(getppid(), SIGINT);
            break;
        }


        if (chunk[i] == '*')
            count++;
        // for each 18-th counted
        if (count == 18)
        {
            count = 0;
            char* line = "******\n";
            for (int j = 0; j < 3; j++){
                bulk_write(fd, line, 7);
            }
            bulk_write(fd, "\n", 1);

            struct timespec t;
            t.tv_nsec = 500000000;
            t.tv_sec = 0;

            while (nanosleep(&t, &t) == -1
                && errno == EINTR) {}
        }
    }
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
    return;
}





void create_children(int num, char* p, sigset_t oldmask){
    pid_t pid;

    int fd;
    if ((fd = TEMP_FAILURE_RETRY(open(p, O_RDONLY))) < 0)
        ERR("open");

#define MAX_STONE 512
    char* chunk = malloc(MAX_STONE * sizeof(char));
    if (chunk == NULL)
        ERR("malloc");


    for (int i = 0; i < num; i++){
        //read a stone
        int bytes_read = 0;
        for (int j = 0; j < 8; j++){
            bytes_read += readline(fd, chunk, bytes_read);
        }
        // We skip the newline with a dummy buffer
        char dummy = ' ';
        bulk_read(fd, &dummy, 1);


        if ((pid = fork()) < 0)
            ERR("fork");
        if (!pid)
        {
            child_work(i, oldmask, chunk, bytes_read, p);
            if (TEMP_FAILURE_RETRY(close(fd)) < 0)
                ERR("close");
            exit(EXIT_SUCCESS);
        }
    }
    free(chunk);
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
}





int main(int argc, char *argv[]) {
    if (argc != 3)
        usage(argc, argv);

    int k = atoi(argv[2]);
    if (k <= 0 || k >= 8)
        usage(argc, argv);
    char *p = argv[1];
    
    
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    sethandler(sig_handler, SIGUSR1);
    sethandler(sig_handler, SIGINT);


    printf("Parent PID: %d\n", getpid());

   
    create_children(k, p, oldmask);

    if (kill(0, SIGUSR1) < 0)
        ERR("kill");


 // waiting for all children
    while (1)
    {
        pid_t pid = wait(NULL);

        if (errno == ECHILD && pid < 0)
            break;
 // SIGINT handling for the parent
        if (errno == EINTR && pid < 0)
        {
            if (last_sig == SIGINT)
            {
                printf("PARENT: received SIGINT!\n");
                if (kill(0, SIGINT) < 0)
                    ERR("kill");
            }
        }
    }
    printf("All workers have gone home\n");

return EXIT_SUCCESS;
}
