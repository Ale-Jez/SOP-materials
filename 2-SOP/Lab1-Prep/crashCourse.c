//Michal github konsultacje
//https://github.com/michau-s/OPS2-L1-Consultations



//randomization
/*
    srand(time(NULL));  // Initialize random seed

    // Define brigades
    struct Brigade brigades[3] = {
        {"Alpha", rand() % 100 + 50, rand() % 5 + 1},  // Random values for capacity and productionRate
        {"Beta", rand() % 100 + 50, rand() % 5 + 1},
        {"Gamma", rand() % 100 + 50, rand() % 5 + 1}
    };
*/



//creating pipes

/*
    int pipe12[2], pipe23[2], pipeB1[2], pipeB2[2], pipeB3[2];

    // Create unnamed pipes
    if (pipe(pipe12) == -1) ERR("pipe12");
    if (pipe(pipe23) == -1) ERR("pipe23");
    if (pipe(pipeB1) == -1) ERR("pipeB1");
    if (pipe(pipeB2) == -1) ERR("pipeB2");
    if (pipe(pipeB3) == -1) ERR("pipeB3");

    pid_t pid;

    // Fork for Brigade 1
    pid = fork();
    if (pid == 0) {
        close(pipe12[0]); // Brigade 1 won't read from pipe12
        close(pipe23[0]); close(pipe23[1]); // Brigade 1 won't use pipe23
        close(pipeB1[0]); // Brigade 1 won't read from pipeB1
        close(pipeB2[0]); close(pipeB2[1]); // Brigade 1 won't use pipeB2
        close(pipeB3[0]); close(pipeB3[1]); // Brigade 1 won't use pipeB3

        // Print message for Brigade 1
        printf("I am Brigade 1. count_descriptors() sees 2 open FDs.\n");
        count_descriptors();
        exit(EXIT_SUCCESS);
    } else if (pid == -1) {
        ERR("fork Brigade 1");
    }

//adjust the logic for any other team
//at the end Parent process closes everything what remained open
//and waits
    while (wait(NULL) > 0);

*/





//child labour should be legalised xD
//1 --> 2 --> 3
/*

void child_work_1(int writeToSecond)
    write
    sleep
    close

void child_work_2(int readFromFirst, int writeToThird)
    read
    write
    sleep
    close x2

void child_work_3(int readFromSecond)
    read
    sleep
    close
*/



//fifo
/*
    if (mkfifo("factory_fifo", 0666) == -1 && errno != EEXIST)
        ERR("mkfifo");
            int fd = open("factory_fifo", O_RDONLY);
    if (fd == -1) ERR("open FIFO");

//or use PIPE_BUF macro (its 4k)
    char buffer[256];
    ssize_t bytesRead;

    while (1) {
        bytesRead = read(fd, buffer, sizeof(buffer));
        if (bytesRead == -1) {
            if (errno == EPIPE) {
                printf("Detected EPIPE, terminating.\n");
                break;
            }
        } else if (bytesRead == 0) {
            break;  // EOF reached
        }
    }

    close(fd);
    
*/




//Handling EPIPE error

/*
    set_handler(SIG_IGN, SIGPIPE);

SIGPIPE is the signal that is sent to a process when it tries to write to a pipe that has no reader
    (e.g., if the other end of the pipe is closed).
SIG_IGN is a special value that means "ignore" the signal.
*/
