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

#define ERR(source) \
    (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

volatile sig_atomic_t last_sig;

ssize_t bulk_read(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
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

void sighandler(int SigNo) { last_sig = SigNo; }

void child_work(char* chunk, int read, sigset_t oldmask, char* p, int i)
{
    printf("CHILD: [%d]\n", getpid());

    // Każdy proces potomny po otrzymaniu sygnału SIGUSR1 rozpoczyna szyfrowanie

    while (last_sig != SIGUSR1)
    {
        sigsuspend(&oldmask);
    }

    // W miarę szyfrowania nowa treść zapisywana jest do pliku <p>-<i>
    char name[100];
    sprintf(name, "%s-%d", p, i);

    int fd;
    if ((fd = TEMP_FAILURE_RETRY(open(name, O_CREAT | O_TRUNC | O_APPEND | O_WRONLY, 0644))) < 0)
        ERR("open");

    for (int j = 0; j < read; j++)
    {
        // Dodaj poprawną obsługę sygnału SIGINT
        if (last_sig == SIGINT)
        {
            // Proces dziecka po otrzymaniu SIGINT [...]
            // wysłać SIGINT do wszystkich innych procesów w grupie
            kill(0, SIGINT);  // Parent ma zablokowany sigint więc nie umrze
            break;
        }

        // szyfrowanie
        if (chunk[j] == 'z')
            chunk[j] = 'c';
        else if (chunk[j] == 'y')
            chunk[j] = 'b';
        else if (chunk[j] == 'x')
            chunk[j] = 'a';
        else
            chunk[j] += 3;

        bulk_write(fd, &chunk[j], 1);

        // Między zamianami kolejnych znaków proces śpi 0.1 sekundy.
        // Poprawnie obsłuż przypadek przerwania snu sygnałem
        struct timespec t;
        t.tv_nsec = 100000000;
        t.tv_sec = 0;

        while (nanosleep(&t, &t) == -1 && errno == EINTR)
        {
        }
    }

    // chunk is freed after child_work finishes, in create_children function
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
}

void parent_work()
{
    printf("PARENT: [%d]\n", getpid());
    return;
}

void create_children(int k, char* p, sigset_t oldmask)
{
    // Pamiętaj aby poprawnie obsłużyć błąd EINTR
    // Chodzi o stosowanie TEMP_FAILURE_RETRY

    pid_t pid;
    struct stat s;

    // Otwórz plik wskazany przez parametr p
    int fd;
    if ((fd = TEMP_FAILURE_RETRY(open(p, O_RDONLY))) < 0)
        ERR("open");

    // Zawartość plików dzielona jest po równo
    if (stat(p, &s) < 0)
        ERR("stat");

    off_t fsize = s.st_size;

    char* chunk = malloc((fsize / k + 1) * sizeof(char));
    if (chunk == NULL)
        ERR("malloc");

    // ostatni proces może mieć kilka znaków mniej lub więcej do przetworzenia.
    // będzie miał zawsze mniej, bulk_read nie wyjdzie poza EOF więc znowu, nic nie trzeba z tym robić

    for (int i = 0; i < k; i++)
    {
        int read;

        // Czytanie zawartości pliku przeplataj z uruchamianiem nowych procesów potomnych tak, aby
        // i-ty proces miał dostęp do i-tego fragmentu pliku.

        // to jest mocna zmyłka, read przesuwa file pointer, więc to się robi samo po każdej iteracji loopa
        // ergo, nic nie trzeba z tym robić

        if ((read = bulk_read(fd, chunk, (fsize / k + 1) * sizeof(char))) == -1)
            ERR("bulk_read");

        if ((pid = fork()) < 0)
            ERR("fork");

        // Child process only
        if (!pid)
        {
            child_work(chunk, read, oldmask, p, i);
            if (TEMP_FAILURE_RETRY(close(fd)) < 0)
                ERR("close");  // children inherit file descriptors
            free(chunk);       // dzieci dostają kopię zaalokowanej pamięci rodzica
            exit(EXIT_SUCCESS);
        }

        // Parent process continues
        // child process exited in if condition
    }

    free(chunk);

    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
}

int main(int argc, char* argv[])
{
    // Program przyjmuje parametry wejściowe opisane przez funkcję usage
    if (argc != 3)
        usage(argc, argv);

    char* p = argv[1];
    int k = atoi(argv[2]);

    if (k <= 0 || k >= 8)
        usage(argc, argv);

    // Blokujemy SIGUSR1 bo kill (0, sigNo) wysyła też do parenta co by zabiło nasz program defaultowo
    // Chcemy żeby tylko dzieci dostały ten sygnał
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    sethandler(sighandler, SIGUSR1);
    sethandler(sighandler, SIGINT);

    create_children(k, p, oldmask);
    // Kiedy wszystkie procesy potomne będą już uruchomione, wyślij do wszystkich sygnał SIGUSR1
    kill(0, SIGUSR1);
    parent_work();

    // Proces rodzic czeka na zakończenie pracy wszystkich procesów dzieci
    while (1)
    {
        pid_t pid = wait(NULL);

        if (errno == ECHILD && pid < 0)
            break;

        // Dodaj poprawną obsługę sygnału SIGINT

        // dostaliśmy siginta, wait ustawi errno na EINTR
        if (errno == EINTR && pid < 0)
        {
            if (last_sig == SIGINT)
                kill(0, SIGINT);  // rodzic się nie zabija sigintem bo ma inny hanlder niż dzieci więc jest git
        }
    }

    return EXIT_SUCCESS;
}
