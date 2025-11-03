## (Non)blocking mode

Visual Aid:

    Blocking = Wait (Blocked)

    Non-Blocking = No Wait (Free)

So when you think of "blocking", remember it’s like a wall that stops you from proceeding until the condition is met (waiting). And when you think of "non-blocking", imagine you’re free to continue without waiting for the queue to be ready.


## Singal usage

The sigevent structure should be filled out depending on the notification type (man 7 sigevent)

For signal-based notification (only realtime signals):
- set sigev_notify to SIGEV_SIGNAL,    
- set sigev_signo to the signal you want to receive (e.g., SIGRTMIN),
- set sigev_value.sival_int or sigev_value.sival_ptr to the value you want to receive in the signal handler argument (an integer or a memory address, respectively).

Additionally:
- when setting the signal handler using the sigaction function, the sigaction structure passed to the function must have the sa_flags field set to SA_SIGINFO (see the sethandler function from the solution in Task 1),
- the values passed in sigev_value are delivered to the handler in the si_value field of the siginfo_t structure (see the mq_handler function in the task solution).

