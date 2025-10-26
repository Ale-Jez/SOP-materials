#include <fcntl.h>  // for O_NONBLOCK
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

void set_nonblocking(mqd_t mqfd) {
    struct mq_attr attr;
    
    // Get the current attributes of the message queue
    if (mq_getattr(mqfd, &attr) == -1) {
        ERR("mq_getattr");
        exit(EXIT_FAILURE);
    }
    
    // Set the non-blocking flag
    attr.mq_flags |= O_NONBLOCK;
    
    // Apply the new attributes (set the queue to non-blocking)
    if (mq_setattr(mqfd, &attr, NULL) == -1) {
        ERR("mq_setattr");
        exit(EXIT_FAILURE);
    }

    //printf("Queue set to non-blocking mode.\n");
}




/*if errno is set to EAGAIN, this means that the queue is empty
(in the case of receiving) or full (in the case of sending) in non-blocking mode.

EAGAIN indicates that the requested operation cannot be completed immediately,
but it’s not a permanent failure — it’s just temporarily unavailable.

In non-blocking mode, instead of waiting for the queue to be ready,
the operation returns immediately with EAGAIN.*/


/*
When you use mq_receive() in non-blocking mode, the function call attempts to retrieve a message from the message queue. If the queue is empty (or if there are no messages available), mq_receive() will return -1 and set errno to EAGAIN.
Understanding EAGAIN:

EAGAIN is a standard error code that indicates a temporary condition
where the operation cannot proceed immediately but might succeed if retried later.
This error code is commonly seen in non-blocking message queues.


In the context of message queues, EAGAIN means that:

    The queue is empty at the moment of the mq_receive() call, and there are no messages to read.

    The queue is operating in non-blocking mode, meaning mq_receive() will not block the calling process if it can't immediately receive a message. Instead, it will return -1 and set errno to EAGAIN.

    The message queue is not "blocking" the process, so the program doesn't wait for a message to arrive and can perform other tasks while waiting for messages.

*/