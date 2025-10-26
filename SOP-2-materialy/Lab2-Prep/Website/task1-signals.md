Task 1: Signal-Based Notification #

Write a program that simulates a simple version of bingo. The parent process randomly draws numbers, and the players are its child processes. Communication between them takes place via POSIX message queues.

The parent process creates n child processes (0 < n < 100, where n is a program argument) and two message queues.
The first queue, pout, is used to send randomly drawn numbers from the range [0,9] to the children once per second.
The second queue, pin, is used to receive messages from the children when they win or complete the game.

Each child process randomly selects its winning number E (in [0,9]) and a number N representing how many numbers it will read from the queue (also from [0,9]). Then, in a loop, the child tries to read numbers from the pout queue — a sent number can only be received by one process, not all of them simultaneously.

Each child compares the received number with its E. If they match, the child sends this number to the parent via the pin queue and exits.
If the child reads N numbers without a match, it sends its own process number (from [1, n]) to the parent via pin before exiting.

The parent process must asynchronously receive messages from the pin queue and print the appropriate output, while still sending numbers every second.
When all child processes terminate, the parent also ends execution and removes both queues.

NOTE: For this task, message size in the queue should be limited to 1 byte!