#define _GNU_SOURCE
#include <bits/types/sigset_t.h>
#include <bits/types/struct_iovec.h>
#include <errno.h>
#include <stdatomic.h>
#include <sys/ucontext.h>
// #include <cerrno>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
// #include <csignal>
#include <signal.h>

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
      return c; // reading error
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
  printf("\tp - path to file to be encrypted\n");
  printf("\t0 < k < 8 - number of child processes\n");
  exit(EXIT_FAILURE);
}

void sethandler(void (*f)(int), int sigNo) {
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = f;
  if (-1 == sigaction(sigNo, &act, NULL))
    ERR("sigaction");
}

void sighandler(int SigNo) { last_sig = SigNo; }




void child_work(char *chunk, int read, sigset_t oldmask, char *p, int i) {
  // part 2
  printf("Child's PID: %d\n", getpid());

  // part 3 ------------------------------------------------
  while (last_sig != SIGUSR1) {
    sigsuspend(&oldmask);
  }

  char name[100];
  // tworzymy nazwę dla pliku, w formacie: <path(p)>-<process number (i)>
  sprintf(name, "%s-%d", p, i);

  /*Opens the file in write mode (O_WRONLY) OR:
      Creates it if it doesn’t exist (O_CREAT).
      Truncates its content if it exists (O_TRUNC).
      Appends data to the file (O_APPEND).
  0644 passed as rwx in octal (-rw-r--r--) owner, group, others*/
  int fd = TEMP_FAILURE_RETRY(
      open(name, O_CREAT | O_TRUNC | O_APPEND | O_WRONLY, 0644));
  if (fd < 0)
    ERR("open");

  for (int j = 0; j < read; j++) {
    // part 4 ------------------------------------------------
    // SIGINT requests that a process terminates, sent by `CTRL + C`.
    if (last_sig == SIGINT) {
      // all processes in the same group will also receive SIGINT
      // kill is JUST a transmitter command
      kill(0, SIGINT);
      break;
    }

    // part 3 ------------------------------------------------
    if (chunk[j] == 'z')
      chunk[j] = 'c';
    else if (chunk[j] == 'y')
      chunk[j] = 'b';
    else if (chunk[j] == 'x')
      chunk[j] = 'a';
    else
      chunk[j] += 3;

    // bulk_write(fd, &chunk[j], sizeof(chunk[j]));
    bulk_write(fd, &chunk[j], 1);

    // sleeping
    struct timespec t;
    t.tv_nsec = 100000000;
    t.tv_sec = 0;

    //&& errno == EINTR here ensures that the failure was caused by a signal
    // interrupting the sleep filters out other potential errors
    while (nanosleep(&t, &t) == -1 && errno == EINTR) {
    }
  }
  if (TEMP_FAILURE_RETRY(close(fd)) < 0)
    ERR("close");
}



void create_children(int k, char *p, sigset_t oldmask) {
  pid_t pid;

  // otwieranie pliku do czytania
  int fd = TEMP_FAILURE_RETRY(open(p, O_RDONLY));
  if (fd < 0) {
    ERR("Open");
  }

  // we want to get the size of this file via stat
  struct stat s;
  if (stat(p, &s) < 0) {
    ERR("stat"); // stat returns 0 at success
  }
  off_t fsize = s.st_size;
  int size = (fsize / k + 1) * sizeof(char); // size of one chunk
  char *chunk = malloc(size);
  if (chunk == NULL)
    ERR("malloc");

  // jak dużo dziecko czyta
  for (int i = 0; i < k; i++) {
    int read = bulk_read(fd, chunk, size);
    if (read == -1)
      ERR("bulk_read");

    // this is the only part of create_children from part 1 (and the for loop)
    if ((pid = fork()) < 0)
      ERR("Fork");

    if (!pid) { // if you're in a child (Denis smirked)
      //! 0 = 1 in here pid returned 0
      child_work(chunk, read, oldmask, p, i);
      if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
      free(chunk);
      exit(EXIT_SUCCESS);
    }
  }
  // happens when fork returned child's pid to the parent - the for loop was
  // exited

  free(chunk);
  if (TEMP_FAILURE_RETRY(close(fd)) < 0)
    ERR("close");
}



void parent_work() { // part2
  printf("Parent's PID: %d\n", getpid());
  return;
}




// argc always has extra + 1 for the command name
int main(int argc, char *argv[]) {
  // when too few arguments passed, couldnt been run
  if (argc != 3)
    usage(argc, argv);

  char *p = argv[1];     // the 1st arg from konsole - path to file
  int k = atoi(argv[2]); // 2nd arg from konsole - value of k

  if (k <= 0 || k >= 8)
    usage(argc, argv);

  // masks for delaying the execution of the signals by processes
  // two sets of signals, named mask and oldmask
  sigset_t mask, oldmask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  sigprocmask(SIG_BLOCK, &mask, &oldmask);
  /*sigprocmask function is used to block the signals in the mask set (in this
  case, SIGUSR1). The SIG_BLOCK action tells the system to block those signals
  from being delivered to the process. The oldmask will store the previous
  signal mask, so you can restore it later.*/

  sethandler(sighandler, SIGUSR1); // part2(?)
  sethandler(sighandler, SIGINT);  // part4

  create_children(k, p, oldmask);
  kill(0, SIGUSR1);
  parent_work();



  while (1) { // waiting loop
    pid_t pid = wait(NULL);
    if (errno == ECHILD && pid < 0) {
      break;
    }

    if (errno == EINTR && pid < 0) {
      if (last_sig == SIGINT) {
        kill(0, SIGINT);
      }
    }
  }

  printf("Parent process terminating\n");

  return EXIT_SUCCESS;
}
