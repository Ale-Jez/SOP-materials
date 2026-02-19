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

ssize_t bulk_write(int fd, char *buf, size_t count) {
  ssize_t c;
  ssize_t len = 0;
  do {
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




int main(int argc, char *argv[]) {
    if (argc != 3)
        usage(argc, argv);

    int k = atoi(argv[2]);
    if (k <= 0 || k >= 8)
        usage(argc, argv);
    //char *p = argv[1];

    printf("Parent PID: %d\n", getpid());


    for  (int i = 0; i < k; i++){
        pid_t pid = fork();
        if (pid < 0)
            ERR("fork");
        else if (pid == 0){
            printf("Child %d PID: %d\n", i, getpid());
            exit(EXIT_SUCCESS); //to ensure that the children won't be executing parents code
        }
    }

    while (wait (NULL) > 0){}

    printf("All workers have gone home\n");

return EXIT_SUCCESS;
}
