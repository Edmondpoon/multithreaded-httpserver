// TODO use epoll
#include "queue.h"
#include "methods.h"
#include "utils.h"
#include "parser.h"
#include "response.h"
#include <assert.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>

#define OPTIONS              "t:l:"
#define DEFAULT_ID 0
#define BUF_SIZE             4096
#define REQUEST_MAX          2048
#define DEFAULT_THREAD_COUNT 4
static FILE *logfile;
#define LOG(...) fprintf(logfile, __VA_ARGS__);

pthread_mutex_t locks[4] = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};
pthread_cond_t conds[4] = { PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER };
enum Lock { LOG, WAIT, FULL, POLL };
enum Cond { WAITING, FILLED, READY };

// working is number of threads currently working
// created is the number of threads that haven't been destroyed (used to ensure all threads close when sigterm)
int working = 0, created = 0;
Queue *queue = NULL;
Queue *polled = NULL;

Queue *paused = NULL;
Queue *writers = NULL;
Queue *readers = NULL;

// Logs a request
void logRequest(int request, char *path, int status, int id) {
    char req[10] = { 0 };
    switch (request) {
    case 0: memcpy(req, "PUT", 3); break;
    case 1: memcpy(req, "APPEND", 6); break;
    case 2: memcpy(req, "GET", 3); break;
    default:;
    }
    pthread_mutex_lock(&(locks[LOG]));
    LOG("%s,%s,%d,%d\n", req, path, status, id);
    fflush(logfile);
    pthread_mutex_unlock(&(locks[LOG]));
    return;
}

// Free regex
void freeRegex(char *words[1024], int size) {
    for (int word = 0; word < size; ++word) {
        if (words[word]) {
            free(words[word]);
            words[word] = NULL;
        }
    }
    return;
}

// Converts a string to an 16 bits unsigned integer.
// Returns 0 if the string is malformed or out of the range.
static size_t strtouint16(char number[]) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT16_MAX || *last != '\0') {
        return 0;
    }
    return num;
}

// Creates a socket for listening for connections.
// Closes the program and prints an error message on error.
static int create_listen_socket(uint16_t port) {
    struct sockaddr_in addr;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        err(EXIT_FAILURE, "socket error");
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof addr) < 0) {
        err(EXIT_FAILURE, "bind error");
    }
    if (listen(listenfd, 128) < 0) {
        err(EXIT_FAILURE, "listen error");
    }
    return listenfd;
}

static void handle_connection(conn *c, regex_t reg) {
    // TODO the implementation of just pushing back to queue is broken rn cause queue may be full
    char *parsed[1024];
    int fd = -1;
    int nonBodyLength = 0, red = 0;
    int connfd = c->fd, matches = 0;
    if (!c->headers_processed) {
        red = read(connfd, c->headers + c->headers_index, REQUEST_MAX - c->headers_index);
        c->headers_index += red;
    }

    // Checks to see if we have encountered every header in the request
    if (!strstr(c->headers, "\r\n\r\n")) {
        if (poll(&c->poller, 1, 1500) == 0) {
            pthread_mutex_lock(&poll_lock);
            push(polled, c);
            pthread_mutex_unlock(&poll_lock);
            return;
        }
        while ((red = read(connfd, c->headers + c->headers_index, REQUEST_MAX - c->headers_index)) > 0) {
            c->headers_index += red;
            // Start looking for the delimiter
            if (!strcmp(c->headers, "\r\n\r\n")) {
                break;
            }
            if (poll(&c->poller, 1, 1500) == 0) {
                pthread_mutex_lock(&poll_lock);
                push(polled, c);
                pthread_mutex_unlock(&poll_lock);
                return;
            }
        }
    }


    /*
    if (!c->headers_processed) {
        bool body = false;
        int temp = red;
        if (poll(&c->poller, 1, 1500) == 0) {
            pthread_mutex_lock(&poll_lock);
            push(polled, c);
            pthread_mutex_unlock(&poll_lock);
            return;
        }
        //printf(" not polled\n");
        int extras = 0;
        while ((red = read(connfd, c->headers + extras, REQUEST_MAX - temp)) > 0) {
            //printf("reading\n");
            temp += red;
            extras += red;
            if (strstr(c->headers, "\r\n\r\n")) {
                body = true;
                break;
            }
            if (poll(&c->poller, 1, 1500) == 0) {
                pthread_mutex_lock(&poll_lock);
                push(polled, c);
                pthread_mutex_unlock(&poll_lock);
                return;
            }
        }
        if (!body) {
            fprintf(stderr, "invalid request (no body)\n");
            regfree(&reg);
            return;
        }
        buffer[extras] = '\0';
    }
    */

    int id = c->id;
    if (!c->headers_processed) {
        matches = regexHeaders(&reg, parsed, c->headers, strlen(c->headers));
        if (matches < 1) {
            fprintf(stderr, "no valid regex match\n");
            freeRegex(parsed, matches);
            return;
        }
        // Helps keep track of whether the buffer includes some body text
        nonBodyLength += strlen(parsed[0]);
        c->method = parseRequestLine(&(c->uri), parsed[0]);
        // parse headers
        // Start at after the first pair of \r\n of the request line
        // Helps keep track of whether the buffer includes some body text
        for (int match = 0; match < matches; ++match) {
            int value = 0;
            int l = strlen(parsed[match]);
            nonBodyLength += l;
            if (l == 2 && parsed[match][0] == '\r' && parsed[match][1] == '\n') {
                // Found the empty header
                c->headers_processed = true;
                break;
            }
            int64_t temp = parseHeaderField(parsed[match], &value);
            if (temp == INVALID) {
                break;
            } else if (c->content_length == 0 && temp == LENGTH) {
                c->content_length = value;
                //printf("getting length %d\n", c->content_length);
            } else if (temp == ID) {
                id = value;
                c->id = value;
            }
        }
        freeRegex(parsed, matches);
    }

    // Path index finds the path given a non-formal path
    if (strlen(c->uri) < 2) {
        // invalid path judged by length
        logRequest(c->method, c->uri, 404, c->id);
        free_fd(&c);
        notFoundResponse(connfd);
        return;
    } else if (!c->tempfile && !parseUri(c->uri, c->method)) {
        // parse URI to deal with directories
        logRequest(c->method, c->uri, 500, c->id);
        free_fd(&c);
        internalErrorResponse(connfd);
        return;
    }

    struct stat sb;
    int exists = stat(c->uri + 1, &sb);
    if (exists < 0 && c->method != PUT) {
        // Put requests are the only ones that don't require the file to exist beforehand
        logRequest(c->method, c->uri, 404, id);
        notFoundResponse(connfd);
    } else if (c->method == GET) {
        int success = Get(c, connfd);
        if (success == OK) {
            logRequest(c->method, c->uri, 200, id);
        }
    } else if (c->method == PUT) {
        int success = Put(c, connfd, nonBodyLength);
        switch (success) {
            case CREATED:
                logRequest(c->method, c->uri, 201, id);
                createdResponse(connfd);
                break;
            case OK:
                logRequest(c->method, c->uri, 200, id);
                okResponse(connfd);
                break;
            case POLL:
                // Don't close the connection for polled requests
                return;
            case ERROR:
                return;
            default:
                return;
        }
    }
    free_fd(&c);
    if (fd > -1) {
        close(fd);
    }
    return;
}

// Handles all logic related to threads
void *thread_handler() {
    struct timeval now = { 0, 0 };
    struct timespec ts = { 0, 0 };
    int a = 0;
    if ((a = pthread_mutex_lock(&(locks[WAIT]))) != 0) {
        printf("what%d\n", a);
        fprintf(stderr, "Failed to lock mutex.\n");
        return NULL;
    }
    regex_t reg;
    if (regcomp(&reg, REQUEST, REG_EXTENDED | REG_ICASE)) {
        fprintf(stderr, "Unable to initialize regular expression.\n");
        exit(EXIT_FAILURE);
    }
    for (;;) {
        while (empty(queue) && !get_cleanup(queue)) {
            // Sleeps until the main thread signals this thread that an incoming request needs parsing
            gettimeofday(&now, NULL);
            ts.tv_sec = now.tv_sec + 5;
            if (pthread_cond_timedwait(&(conds[WAITING]), &(locks[WAIT]), &ts) == ETIMEDOUT && !empty(polled)) {
                pthread_mutex_lock(&poll_lock);
                //if (!empty(polled)) {
                for (int i = 0; i < length(polled); ++i) {
                    // TODO assumes doesnt get full
                    conn *temp = pop(polled);
                    if (poll(&temp->poller, 1, 1500) > 0) {
                        push(queue, temp);
                    } else {
                        push(polled, temp);
                    }
                }
                pthread_mutex_unlock(&poll_lock);
            }
        }
        if (get_cleanup(queue)) {
            break;
        }
        conn *client = pop(queue);
        pthread_cond_signal(&(conds[FILLED]));
        working += 1;
        if (pthread_mutex_unlock(&(locks[WAIT])) != 0) {
            fprintf(stderr, "Failed to unlock mutex.\n");
            return NULL;
        }

        handle_connection(client, reg);
        if (pthread_mutex_lock(&(locks[WAIT])) != 0) {
            fprintf(stderr, "Failed to lock mutex.\n");
            return NULL;
        }
        working -= 1;
    }
    created -= 1;
    pthread_cond_signal(&(conds[READY]));
    if (pthread_mutex_unlock(&(locks[WAIT])) != 0) {
        fprintf(stderr, "Failed to unlock mutex.\n");
        return NULL;
    }
    regfree(&reg);
    return NULL;
}

static void sigterm_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        if (pthread_mutex_lock(&(locks[WAIT])) != 0) {
            warn("Failed to lock mutex.\n");
        }
        cleanup_init(queue);
        int stopping = created;
        if (pthread_mutex_unlock(&(locks[WAIT])) != 0) {
            warn("Failed to unlock mutex.\n");
        }
        while (stopping > 0) {
            // signals to every thread that we no longer need them
            // has the threads end their loops
            if (pthread_mutex_lock(&(locks[WAIT])) != 0) {
                warn("Failed to lock mutex.\n");
            }
            int current = created;
            pthread_cond_signal(&(conds[WAITING]));
            while (created == current) {
                pthread_cond_wait(&(conds[READY]), &(locks[WAIT]));
            }
            stopping -= 1;
            if (pthread_mutex_unlock(&(locks[WAIT])) != 0) {
                warn("Failed to unlock mutex.\n");
            }
        }
        if (queue) {
            free_queue(&queue);
        }
        if (polled) {
            free_queue(&polled);
        }
        for (int l = 0; l < 4; ++l) {
            pthread_mutex_destroy(&(locks[l]));
            pthread_cond_destroy(&(conds[l]));
        }
        /*
        pthread_mutex_destroy(&wait_lock);
        pthread_cond_destroy(&ready);
        pthread_cond_destroy(&filled);
        */
        warnx("received SIGTERM");
        fclose(logfile);
        exit(EXIT_SUCCESS);
    }
}

static void usage(char *exec) {
    fprintf(stderr, "usage: %s [-t threads] [-l logfile] <port>\n", exec);
}

int main(int argc, char *argv[]) {
    int opt = 0;
    int threads = DEFAULT_THREAD_COUNT;
    logfile = stderr;
    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
        case 't':
            threads = strtol(optarg, NULL, 10);
            if (threads <= 0) {
                errx(EXIT_FAILURE, "bad number of threads");
            }
            break;
        case 'l':
            logfile = fopen(optarg, "w");
            if (!logfile) {
                errx(EXIT_FAILURE, "bad logfile");
            }
            break;
        default: usage(argv[0]); return EXIT_FAILURE;
        }
    }
    if (optind >= argc) {
        warnx("wrong number of arguments");
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    uint16_t port = strtouint16(argv[optind]);
    if (port == 0) {
        errx(EXIT_FAILURE, "bad port number: %s", argv[1]);
    }
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, sigterm_handler);

    queue = create_queue(BUF_SIZE);
    assert(queue);

    polled = create_queue(BUF_SIZE); // Stores polled requests
    assert(polled);

    pthread_t workers[threads];

    created = threads;
    for (int i = 0; i < threads; i++) {
        // Initializing all threads
        if (pthread_create(workers + i, NULL, thread_handler, NULL) != 0) {
            warnx("Failed to create a thread.\n");
            return EXIT_FAILURE;
        }
    }

    int listenfd = create_listen_socket(port);
    for (;;) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            warn("accept error");
            continue;
        }
        assert(pthread_mutex_lock(&(locks[WAIT])) == 0);
        while (full(queue)) {
            // Full bounded queue so the current client has to be put on hold
            pthread_cond_wait(&(conds[FILLED]), &(locks[WAIT]));
        }
        conn *c = create_fd(connfd, DEFAULT_ID, 0, -1);
        if (poll(&c->poller, 1, 1500) == 0) {
            if (full(polled)) {
                printf("full\n");
            }
            pthread_mutex_lock(&poll_lock);
            push(polled, c);
            pthread_mutex_unlock(&poll_lock);
        } else {
            push(queue, c);
        }

        int l = length(queue);
        int max = (l > threads - working ? threads - working : l);
        for (int i = 0; i < max; ++i) {
            pthread_cond_signal(&(conds[WAITING]));
        }

        assert(pthread_mutex_unlock(&(locks[WAIT])) == 0);
    }
    return EXIT_SUCCESS;
}
