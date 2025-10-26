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


void usage(int argc, char *argv[]) {
  printf("%s p k \n", argv[0]);
  printf("\tp - path to file with stones\n");
  printf("\t0 < k < 8 - number of child processes\n");
  exit(EXIT_FAILURE);
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


void child_work(int idx){
    printf("Child's ID: %d\n", idx);
    return;
}





void create_children(int num){
    pid_t pid;

    for (int i = 0; i < num; i++){
        if ((pid = fork()) < 0)
            ERR("fork");
        if (!pid)
        {
            child_work(i);
            exit(EXIT_SUCCESS);
        }
    }
    }





int main(int argc, char *argv[]) {
    if (argc != 3)
        usage(argc, argv);

    int k = atoi(argv[2]);
    if (k <= 0 || k >= 8)
        usage(argc, argv);
    // /char *p = argv[1];
    
    
    printf("Parent PID: %d\n", getpid());

   
    create_children(k);




 // waiting for all children
    while (1)
    {
        pid_t pid = wait(NULL);

        if (errno == ECHILD && pid < 0)
            break;

        
    }



    printf("All workers have gone home\n");

return EXIT_SUCCESS;
}
