#include "common.h"

#define MAX_CLIENTS 4
#define MAX_USERNAME_LENGTH 32
#define MAX_MESSAGE_SIZE 64
#define MAX_EVENTS (MAX_CLIENTS + 1)

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

void block_sigpipe()
{
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
}
void get_and_check_args(int argc, char** argv, uint16_t* port)
{
    if (argc != 2)
    {
        usage(argv[0]);
    }
    *port = atoi(argv[1]);

    if (*port <= 1023 || *port >= 65535)
    {
        usage(argv[0]);
    }
}

void decline_new_user(int fd)
{
    if (write(fd, "Server is full\n", 16) < 0)
    {
        if (errno != EPIPE)
        {
            ERR("write");
        }
        if (TEMP_FAILURE_RETRY(close(fd)) < 0)
            ERR("close");
    }
}

void add_new_user_to_list(int client_socket, int epoll_ds, user_context* client_list, int* current_connections_number)
{
    user_context new_user_context = {
        .offset = 0,
        .user_fd = client_socket,
    };

    client_list[(*current_connections_number)] = new_user_context;
    (*current_connections_number)++;
    int size;

    printf("[%d] connected\n", client_socket);
    for (int i = 0; i < (*current_connections_number) - 1; i++)
    {
        if (write(client_socket, "User logging in...\n", 20) < 0)

            if (errno != EPIPE)
            {
                ERR("write");
            }
    }

    if (write(client_socket, "Please enter your username\n", 28) < 0)
    {
        if (errno != EPIPE)
        {
            ERR("write");
        }
    }

    struct epoll_event user_event;
    user_event.events = EPOLLIN;
    user_event.data.fd = client_socket;
    if (epoll_ctl(epoll_ds, EPOLL_CTL_ADD, client_socket, &user_event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }
}

void known_user_handler(struct epoll_event client_event)
{
    char buf[MAX_MESSAGE_SIZE + 1] = {0};
    int read_chars;
    read_chars = read(client_event.data.fd, buf, MAX_MESSAGE_SIZE);
    if (read_chars == 0)
    {
        if (TEMP_FAILURE_RETRY(close(client_event.data.fd)) < 0)
            ERR("close");
    }

    if (read_chars < 0)
    {
        ERR("read");
    }
    printf("%s\n", buf);
}

int main(int argc, char** argv)
{
    block_sigpipe();

    uint16_t port;
    get_and_check_args(argc, argv, &port);

    int server_socket = bind_tcp_socket(port, 16);

    int new_flags = fcntl(server_socket, F_GETFL) | O_NONBLOCK;
    fcntl(server_socket, F_SETFL, new_flags);

    int epoll_ds;

    if ((epoll_ds = epoll_create1(0)) < 0)
    {
        ERR("epoll_create:");
    }

    user_context client_list[MAX_CLIENTS];
    int current_connections_number = 0;

    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_ds, EPOLL_CTL_ADD, server_socket, &event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        int ready_fds = epoll_wait(epoll_ds, events, MAX_EVENTS, -1);

        for (int i = 0; i < ready_fds; i++)
        {
            if (events[i].data.fd == server_socket)
            {
                // new user
                int client_socket = add_new_client(events[i].data.fd);
                if (current_connections_number >= MAX_CLIENTS)
                {
                    // we do not add a new user (the limit is exceeded)
                    decline_new_user(events[i].data.fd);
                }
                else
                {
                    // we add a new user (the limit is not exceeded)
                    add_new_user_to_list(client_socket, epoll_ds, client_list, &current_connections_number);
                }
            }
            else
            {
                // message from already connected user
                known_user_handler(events[i]);
            }
        }
    }

    if (TEMP_FAILURE_RETRY(close(server_socket)) < 0)
        ERR("close");

    return EXIT_SUCCESS;
}
