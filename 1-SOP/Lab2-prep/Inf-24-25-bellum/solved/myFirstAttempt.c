#include <bits/types/struct_iovec.h>
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
//#include <cerrno>
//#include <csignal>
#include <signal.h>


#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t last_sig;

ssize_t bulk_read(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c; //reading error
        if (c == 0)
            return len;  // EOF
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

void usage(int argc, char* argv[])
{
    printf("%s p k \n", argv[0]);
    printf("\tp - path to file to be encrypted\n");
    printf("\t0 < k < 8 - number of child processes\n");
    exit(EXIT_FAILURE);
}


void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}



void create_children(int fd, int k, size_t file_size){
    size_t chunk_size = file_size / k;
    size_t remainder = file_size % k;

    for (int i =0; i < k; i++){
        pid_t pid = fork(); 
        //child gets 0, parent gets child's pid
        //pid should be zero

        if (pid < 0){ //pid = -1 when fork failes
            ERR("Fork failed");
        }
        else if( pid == 0){ //for each child process
            printf("Child proccess PID: %d\n", getpid());
            
            // Create a duplicate file descriptor for this child
            int child_fd = dup(fd);
            if (child_fd < 0){
                ERR("Failed to duplicate file descriptor");
            }

            size_t offset = i * chunk_size;
            size_t read_size;
            if (i == k-1){ //last chunk gets the remainder
                read_size = chunk_size + remainder;
            }
            else{
                read_size = chunk_size;
            }

            char* buffer = malloc(read_size);
            if(!buffer) ERR("Memory allocation failed\n");
            
            if (-1 == lseek(child_fd, offset, SEEK_SET)){
                ERR("Lseek failed");
            }

            //writing to standard output
            ssize_t bytes_read = bulk_read(child_fd, buffer, read_size);
            if (bytes_read < 0){
                ERR("Bulk Read failed");
            }


            if (bulk_write(child_fd, buffer, bytes_read) < 0){
                ERR("Bulk write failed");
            }
            printf("\n");
            free(buffer);

            exit(EXIT_SUCCESS);
        }
    }
}





//argc always has extra +1 for the command name
int main(int argc, char* argv[])
{
    //when too few arguments passed, couldnt been run
    if (argc != 3){
        usage(argc, argv);
    }

    char* file_path = argv[1]; //the 1st arg from konsole - path to file
    int k = atoi(argv[2]); //2nd arg from konsole - value of k
    
    if ( 0 >= k || k >= 8 ){
        usage(argc, argv);
    }

    int fd = open (file_path, O_RDONLY);
    if (fd < 0){
        ERR("Failed to open\n");
    }

    //needed to obtain file's size
    struct stat file_stat;
    if (-1 == fstat(fd, &file_stat)){
        ERR("Fstat error");
    }
    size_t file_size = file_stat.st_size;
    if (0 == file_size){
        fprintf(stderr, "Error: File is empty");
        close(fd);
        exit(EXIT_FAILURE);
    }


    printf("Parent's PID: %d\n", getpid());
    

    create_children(fd, k, file_size);    



    while(1){ //waiting loop
        pid_t pid = wait(NULL);
        if (errno == ECHILD && pid < 0){
            break;
        }

        if (errno == EINTR && pid < 0){
            if (last_sig == SIGINT){
                kill(0, SIGINT);
            }
        }
    }

    printf("Parent process terminating\n");


    return EXIT_SUCCESS;
}
