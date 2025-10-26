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

#define MAX_STONE 512

volatile sig_atomic_t sigusr1_received = 0;

void sigusr1_handler (int signo){
    sigusr1_received = 1;
}



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




void read_stones(char* p, char* stone, int stone_idx) {
    #define MAX_LINE_LEN 1024    
    int fd = open(p, O_RDONLY);
    if (fd < 0)
        ERR("open");


    if (lseek(fd, stone_idx * MAX_STONE, SEEK_SET) < 0)
            ERR("lseek");

//9 times because they are separated by the /n
    for (int i = 0; i < 9; i++){

        char line[MAX_LINE_LEN];
        int line_size = 0;
        char c;
        while (TEMP_FAILURE_RETRY(read(fd, &c, 1)) == 1) {
            if (c == '\n' || line_size >= MAX_LINE_LEN - 1) {
                line[line_size++] = c;
                break;
            }
            line[line_size++] = c;
        }

        if (line_size == 0)
            break;

//adding a line to the buffer for stone
        strcat(stone, line);
    }
}


void child_work(char* p, int num){
//register SIGUSR1 handler
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) < 0)
        ERR("sigaction");



    //buffer initialized to 0
    char buff[MAX_STONE] = {0};
    read_stones(p, buff, num);
    
    //wait for SIGUSR1
    while (!sigusr1_received){
        pause(); //sleep until signal is received
    }
   
    exit(EXIT_SUCCESS); //to ensure that the children won't be executing parents code, nor each others
        

}





int main(int argc, char *argv[]) {
    if (argc != 3)
        usage(argc, argv);

    int k = atoi(argv[2]);
    if (k <= 0 || k >= 8)
        usage(argc, argv);
    char *p = argv[1];
    
    
    printf("Parent PID: %d\n", getpid());

   

    pid_t children[k];

    for (int i = 0; i < k; i++){
        pid_t pid = fork();
        if (pid < 0)
            ERR("fork");
        else if (pid == 0){
            printf("Child %d PID: %d\n", i, getpid());
            child_work(p, i);
        }


        children[i] = pid;
    }
    sleep(1);

    for (int i = 0; i < k; i++){
        if (kill (children[i], SIGUSR1) < 0)
            ERR("kill");
    }

    while (wait (NULL) > 0){}

    printf("All workers have gone home\n");

return EXIT_SUCCESS;
}
