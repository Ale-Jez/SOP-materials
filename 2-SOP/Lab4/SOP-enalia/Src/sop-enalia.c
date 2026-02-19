#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include "common.h"

#define MAX_CLIENTS 4
#define MAX_USERNAME_LENGTH 32
#define MAX_MESSAGE_SIZE 64
#define MAX_EVENTS 64


typedef struct user_context
{
    char username[MAX_USERNAME_LENGTH];
    char buf[MAX_MESSAGE_SIZE];
    int offset;
    int user_fd;
} user_context;


void usage(char* program_name)
{
    fprintf(stderr, "Usage: \n");

    fprintf(stderr, "\t%s", program_name);
    set_color(2, SOP_PINK);
    fprintf(stderr, " port\n");

    fprintf(stderr, "\t  port");
    reset_color(2);
    fprintf(stderr, " - the port on which the server will run\n");

    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    if (argc != 2)
        usage(argv[0]);

    uint16_t port;
    port = atoi(argv[1]);
    if (port <= 10 || port >= 65535)
        usage(argv[0]);

    int socket = make_tcp_socket();
    make_address("0.0.0.0", argv[1]);
    int server_socket = bind_tcp_socket(port, 16);
    int epoll_ds = epoll_create1(0);
    if (epoll_ds < 0)
        ERR("epoll_create1");


    user_context client_list[MAX_CLIENTS];
    int current_connections_number = 0;

    
    struct epoll_event event, events[MAX_CLIENTS];
    event.events = EPOLLIN;
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_ds,  EPOLL_CTL_ADD, server_socket, &event) == -1)
        ERR("epoll_ctl");


    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    while(1){
        int ready_fds = epoll_wait(epoll_ds, events,MAX_EVENTS,-1);

        for (int i = 0; i < ready_fds; i++)
        {
            int client_socket = add_new_client(events[i].data.fd);
            if(client_socket == server_socket)
            {
                //new user
                if (current_connections_number >= MAX_CLIENTS)
                {
                    if (write(client_socket, "Goodbye\n", 9) < 0)
                        if (errno != EPIPE)
                            ERR("write");   
                    if (TEMP_FAILURE_RETRY(close(client_socket)) < 0)
                        ERR("close");
                }
                
                user_context new_user_context = {
                    .offset = 0,
                    .user_fd = client_socket,
                };
                client_list[current_connections_number] = new_user_context;
                current_connections_number++;

                printf("Client sockets: %d |\n", client_socket);
                while (1) {
                    char buf;
                    int read_chars = read(client_socket, &buf, 1);
                    if (read_chars == 0)
                        break;
                    if (read_chars < 0)
                        ERR("read");

                    printf("%c\n", buf);
                    if (buf == '\n')
                        break;
                }

                if (write(client_socket, "Hello World\n", 13) < 0)
                    if (errno != EPIPE)
                        ERR("write");   
                if (TEMP_FAILURE_RETRY(close(client_socket)) < 0)
                    ERR("close");        
            }
            else
            {
                //message from already connectes user
            }


        }
    }    


    if (TEMP_FAILURE_RETRY(close(server_socket)) < 0)
        ERR("close");  

    return EXIT_SUCCESS;
}
