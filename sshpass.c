#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define VERSION "1.0"
#define DEFAULT_PROMPT "assword:"
#define HOST_KEY_PROMPT "The authenticity of host "

enum { EX_USAGE = 1, EX_CONFLICT = 2, EX_RUNTIME = 3, EX_PARSE = 4,
       EX_BADPASS = 5, EX_HOSTKEY = 6 };

static const char *program_name = "sshpass";
static bool verbose;
static volatile sig_atomic_t caught_signal;

static void
log_message(const char *format, ...)
{
    va_list args;

    if (!verbose)
        return;
    fprintf(stderr, "%s: ", program_name);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
}

static void
usage(FILE *stream)
{
    fprintf(stream,
        "Usage: %s [-f filename|-d fd|-p password|-e] [-P prompt] [-v] command [args...]\n"
        "       %s -h | -V\n\n"
        "  -f file      read password from the first line of file\n"
        "  -d fd        read password from an already open file descriptor\n"
        "  -p password  password on command line (visible to other users)\n"
        "  -e           read password from SSHPASS environment variable\n"
        "  -P prompt    use a custom prompt instead of Password:/password:\n"
        "  -v           verbose diagnostics\n",
        program_name, program_name);
}

static void
on_signal(int signal_number)
{
    caught_signal = signal_number;
}

static int
install_signal_handlers(void)
{
    struct sigaction action;
    int signals[] = { SIGHUP, SIGINT, SIGQUIT, SIGTERM };
    size_t i;

    memset(&action, 0, sizeof(action));
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    for (i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
        if (sigaction(signals[i], &action, NULL) < 0)
            return -1;
    }
    signal(SIGPIPE, SIG_IGN);
    return 0;
}

static char *
read_password_fd(int fd)
{
    size_t used = 0, capacity = 128;
    char *password = malloc(capacity);

    if (password == NULL)
        return NULL;
    for (;;) {
        char c;
        ssize_t count = read(fd, &c, 1);

        if (count == 0 || (count == 1 && (c == '\n' || c == '\r')))
            break;
        if (count < 0) {
            if (errno == EINTR)
                continue;
            free(password);
            return NULL;
        }
        if (used + 1 >= capacity) {
            char *larger;
            capacity *= 2;
            larger = realloc(password, capacity);
            if (larger == NULL) {
                memset(password, 0, used);
                free(password);
                return NULL;
            }
            password = larger;
        }
        password[used++] = c;
    }
    password[used] = '\0';
    return password;
}

static int
write_all(int fd, const void *data, size_t size)
{
    const char *cursor = data;

    while (size > 0) {
        ssize_t written = write(fd, cursor, size);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        cursor += written;
        size -= (size_t)written;
    }
    return 0;
}

/* Feed one byte into a small streaming substring matcher. */
static bool
match_prompt(char c, const char *prompt, size_t *matched)
{
    size_t length = strlen(prompt);

    if (length == 0)
        return true;
    if (c == prompt[*matched]) {
        (*matched)++;
        if (*matched == length) {
            *matched = 0;
            return true;
        }
    } else {
        *matched = (c == prompt[0]) ? 1 : 0;
    }
    return false;
}

static int
open_pty(char **slave_name)
{
    int master = posix_openpt(O_RDWR | O_NOCTTY);

    if (master < 0 || grantpt(master) < 0 || unlockpt(master) < 0) {
        if (master >= 0)
            close(master);
        return -1;
    }
    *slave_name = ptsname(master);
    if (*slave_name == NULL) {
        close(master);
        return -1;
    }
    return master;
}

static void
run_child(int master, const char *slave_name, char **command)
{
    int slave;

    close(master);
    if (setsid() < 0)
        _exit(126);
    slave = open(slave_name, O_RDWR);
    if (slave < 0)
        _exit(126);
#ifdef TIOCSCTTY
    (void)ioctl(slave, TIOCSCTTY, 0);
#endif
    if (dup2(slave, STDIN_FILENO) < 0 || dup2(slave, STDOUT_FILENO) < 0
        || dup2(slave, STDERR_FILENO) < 0)
        _exit(126);
    if (slave > STDERR_FILENO)
        close(slave);
    execvp(command[0], command);
    dprintf(STDERR_FILENO, "%s: cannot run %s: %s\n", program_name,
        command[0], strerror(errno));
    _exit(errno == ENOENT ? 127 : 126);
}

static int
relay(int master, pid_t child, const char *password, const char *prompt)
{
    struct pollfd descriptors[2];
    size_t matched = 0;
    size_t host_key_matched = 0;
    unsigned prompt_count = 0;
    bool stdin_open = true, master_open = true;
    char buffer[4096];

    while (master_open) {
        int ready;

        if (caught_signal != 0) {
            kill(child, caught_signal);
            caught_signal = 0;
        }
        descriptors[0].fd = master;
        descriptors[0].events = POLLIN;
        descriptors[0].revents = 0;
        descriptors[1].fd = stdin_open ? STDIN_FILENO : -1;
        descriptors[1].events = POLLIN;
        descriptors[1].revents = 0;
        ready = poll(descriptors, 2, -1);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            return EX_RUNTIME;
        }
        if (descriptors[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            ssize_t count = read(master, buffer, sizeof(buffer));
            ssize_t i;

            if (count <= 0) {
                if (count < 0 && errno == EINTR)
                    continue;
                /* Linux PTYs report EIO at slave close; Haiku may return EOF. */
                master_open = false;
            } else {
                (void)write_all(STDOUT_FILENO, buffer, (size_t)count);
                for (i = 0; i < count; i++) {
                    if (match_prompt(buffer[i], HOST_KEY_PROMPT,
                            &host_key_matched)) {
                        log_message("unknown host key prompt detected");
                        kill(child, SIGTERM);
                        return EX_HOSTKEY;
                    }
                    if (match_prompt(buffer[i], prompt, &matched)) {
                        prompt_count++;
                        if (prompt_count > 1) {
                            log_message("password prompt seen again; authentication failed");
                            kill(child, SIGTERM);
                            return EX_BADPASS;
                        }
                        log_message("password prompt detected");
                        if (write_all(master, password, strlen(password)) < 0
                            || write_all(master, "\n", 1) < 0)
                            return EX_RUNTIME;
                    }
                }
            }
        }
        if (stdin_open && (descriptors[1].revents & POLLIN)) {
            ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (count > 0) {
                if (write_all(master, buffer, (size_t)count) < 0)
                    stdin_open = false;
            } else if (count == 0) {
                stdin_open = false;
            } else if (errno != EINTR) {
                stdin_open = false;
            }
        }
        if (stdin_open && (descriptors[1].revents & (POLLHUP | POLLERR)))
            stdin_open = false;
    }
    return 0;
}

int
main(int argc, char **argv)
{
    const char *prompt = DEFAULT_PROMPT;
    const char *password_argument = NULL;
    char *owned_password = NULL;
    int source_count = 0, password_fd = -1, option, master, status, relay_status;
    char *slave_name;
    pid_t child, waited;

    if (argv[0] != NULL)
        program_name = argv[0];
    while ((option = getopt(argc, argv, "+f:d:p:eP:vhV")) != -1) {
        switch (option) {
        case 'f':
            source_count++;
            password_fd = open(optarg, O_RDONLY);
            if (password_fd < 0) {
                fprintf(stderr, "%s: cannot open %s: %s\n", program_name,
                    optarg, strerror(errno));
                return EX_RUNTIME;
            }
            break;
        case 'd': {
            char *end;
            long descriptor;

            source_count++;
            errno = 0;
            descriptor = strtol(optarg, &end, 10);
            if (errno != 0 || *optarg == '\0' || *end != '\0'
                || descriptor < 0 || descriptor > 0x7fffffffL) {
                fprintf(stderr, "%s: invalid file descriptor: %s\n", program_name, optarg);
                return EX_USAGE;
            }
            password_fd = (int)descriptor;
            break;
        }
        case 'p':
            source_count++;
            owned_password = strdup(optarg);
            if (owned_password == NULL) {
                fprintf(stderr, "%s: cannot copy password: %s\n",
                    program_name, strerror(errno));
                return EX_RUNTIME;
            }
            memset(optarg, 'z', strlen(optarg));
            password_argument = owned_password;
            break;
        case 'e': source_count++; password_argument = getenv("SSHPASS"); break;
        case 'P': prompt = optarg; break;
        case 'v': verbose = true; break;
        case 'V': printf("sshpass %s\n", VERSION); return 0;
        case 'h': usage(stdout); return 0;
        default: usage(stderr); return EX_USAGE;
        }
    }
    if (source_count > 1) {
        fprintf(stderr, "%s: only one password source may be specified\n", program_name);
        return EX_CONFLICT;
    }
    if (optind >= argc) {
        usage(stderr);
        return EX_USAGE;
    }
    if (source_count == 0) {
        password_fd = STDIN_FILENO;
        log_message("reading password from standard input");
    }
    if (password_fd >= 0) {
        owned_password = read_password_fd(password_fd);
        if (password_fd != STDIN_FILENO)
            close(password_fd);
        if (owned_password == NULL) {
            fprintf(stderr, "%s: could not read password: %s\n", program_name, strerror(errno));
            return EX_RUNTIME;
        }
        password_argument = owned_password;
    } else if (password_argument == NULL) {
        fprintf(stderr, "%s: SSHPASS is not set\n", program_name);
        return EX_USAGE;
    }

    master = open_pty(&slave_name);
    if (master < 0) {
        fprintf(stderr, "%s: cannot allocate pseudo-terminal: %s\n",
            program_name, strerror(errno));
        return EX_RUNTIME;
    }
    if (install_signal_handlers() < 0) {
        fprintf(stderr, "%s: cannot install signal handlers: %s\n", program_name, strerror(errno));
        close(master);
        return EX_RUNTIME;
    }
    child = fork();
    if (child < 0) {
        fprintf(stderr, "%s: cannot fork: %s\n", program_name, strerror(errno));
        close(master);
        return EX_RUNTIME;
    }
    if (child == 0)
        run_child(master, slave_name, &argv[optind]);

    relay_status = relay(master, child, password_argument, prompt);
    close(master);
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (owned_password != NULL) {
        memset(owned_password, 0, strlen(owned_password));
        free(owned_password);
    }
    if (relay_status != 0)
        return relay_status;
    if (waited < 0)
        return EX_RUNTIME;
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return EX_RUNTIME;
}
