#include "client.h"
#include "methods.h"
#include "parser.h"
#include "queue.h"
#include "response.h"
#include "utils.h"
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>

#define OPTIONS              "t:l:"
#define BUF_SIZE             4096
#define MAX_EVENTS           4096
#define REQUEST_MAX          2048
#define DEFAULT_THREAD_COUNT 4
static FILE *logfile;
#define LOG(...) fprintf(logfile, __VA_ARGS__);

pthread_mutex_t locks[4] = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER };
pthread_cond_t conds[4] = { PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER,
    PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER };
enum Lock { LOG, WAIT, FULL, POLL };
enum Cond { WAITING, FILLED, READY };

// working is number of threads currently working
// created is the number of threads that haven't been destroyed (used to ensure all threads close when sigterm)
int working = 0, created = 0;
Queue *queue = NULL;
Queue *polled = NULL;
pthread_t *workers = NULL;

// Logs a request
void log_request(int request, char *path, int status, int id) {
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
void free_regex(char *words[1024], int size) {
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

static void handle_connection(Client *connection, regex_t reg) {
    // TODO the implementation of just pushing back to queue is broken rn cause queue may be full
    char *parsed[1024];
    int nonBodyLength = 0, red = 0;
    int connfd = connection->fd, matches = 0;
    if (!connection->headers_processed) {
        red = read(connfd, connection->headers + connection->headers_index,
            REQUEST_MAX - connection->headers_index);
        connection->headers_index += red;
    }

    // Checks to see if we have encountered every header in the request
    if (!strstr(connection->headers, "\r\n\r\n")) {
        if (poll_client(connection, polled)) {
            return;
        }
        while ((red = read(connfd, connection->headers + connection->headers_index,
                    REQUEST_MAX - connection->headers_index))
               > 0) {
            connection->headers_index += red;
            // Start looking for the delimiter
            if (!strcmp(connection->headers, "\r\n\r\n")) {
                break;
            }
            if (poll_client(connection, polled)) {
                return;
            }
        }
    }

    int id = connection->id;
    if (!connection->headers_processed) {
        matches = regex_headers(&reg, parsed, connection->headers, strlen(connection->headers));
        if (matches < 1) {
            fprintf(stderr, "no valid regex match\n");
            free_regex(parsed, matches);
            return;
        }
        // Helps keep track of whether the buffer includes some body text
        nonBodyLength += strlen(parsed[0]);
        set_method(connection, parse_requestLine(&(connection->uri), parsed[0]));
        // parse headers
        // Start at after the first pair of \r\n of the request line
        // Helps keep track of whether the buffer includes some body text
        for (int match = 1; match < matches; ++match) {
            int value = 0;
            int l = strlen(parsed[match]);
            nonBodyLength += l;
            if (l == 2 && parsed[match][0] == '\r' && parsed[match][1] == '\n') {
                // Found the empty header
                connection->headers_processed = true;
                break;
            }
            int64_t temp = parse_headerField(parsed[match], &value);
            if (temp == INVALID) {
                break;
            } else if (connection->content_length == UNDEFINED && temp == LENGTH) {
                set_length(connection, value);
            } else if (temp == ID) {
                id = value;
                set_id(connection, value);
            }
        }
        free_regex(parsed, matches);
    }

    // Path index finds the path given a non-formal path
    if (strlen(connection->uri) < 2) {
        // invalid path judged by length
        log_request(connection->method, connection->uri, 404, connection->id);
        close_client(&connection);
        notFoundResponse(connfd);
        return;
    } else if (!strcmp(connection->tempfile, "XXXXXX")
               && !parse_uri(connection->uri, connection->method)) {
        // parse URI to deal with directories
        log_request(connection->method, connection->uri, 500, connection->id);
        close_client(&connection);
        internalErrorResponse(connfd);
        return;
    }

    struct stat sb;
    int exists = stat(connection->uri + 1, &sb);
    if (exists < 0 && connection->method != PUT) {
        // Put requests are the only ones that don't require the file to exist beforehand
        log_request(connection->method, connection->uri, 404, id);
        notFoundResponse(connfd);
    } else if (connection->method == GET) {
        int success = Get(connection, connfd);
        if (success == OK) {
            log_request(connection->method, connection->uri, 200, id);
        }
    } else if (connection->method == PUT) {
        int success = Put(connection, nonBodyLength, polled);
        switch (success) {
        case CREATED: log_request(connection->method, connection->uri, 201, id); break;
        case OK: log_request(connection->method, connection->uri, 200, id); break;
        case POLL:
            // Don't close the connection for polled requests
            return;
        case ERROR: return;
        default: return;
        }
    }
    close_client(&connection);
    return;
}

// Handles all logic related to threads
void *thread_handler() {
    struct timeval now = { 0, 0 };
    struct timespec ts = { 0, 0 };
    assert(pthread_mutex_lock(&(locks[WAIT])) == 0);
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
            if (pthread_cond_timedwait(&(conds[WAITING]), &(locks[WAIT]), &ts) == ETIMEDOUT
                && !empty(polled)) {
                pthread_mutex_lock(&poll_lock);
                //if (!empty(polled)) {
                for (int i = 0; i < length(polled); ++i) {
                    // TODO assumes doesnt get full
                    Client *temp = pop(polled);
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
        Client *client = pop(queue);
        pthread_cond_signal(&(conds[FILLED]));
        working += 1;
        assert(pthread_mutex_unlock(&(locks[WAIT])) == 0);

        handle_connection(client, reg);
        assert(pthread_mutex_lock(&(locks[WAIT])) == 0);
        working -= 1;
    }
    created -= 1;
    pthread_cond_signal(&(conds[READY]));
    assert(pthread_mutex_unlock(&(locks[WAIT])) == 0);
    regfree(&reg);
    return NULL;
}

static void sigterm_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        assert(pthread_mutex_lock(&(locks[WAIT])) == 0);
        cleanup_init(queue);
        int stopping = created, total_threads = created;
        assert(pthread_mutex_unlock(&(locks[WAIT])) == 0);
        while (stopping > 0) {
            // signals to every thread that we no longer need them
            // has the threads end their loops
            assert(pthread_mutex_lock(&(locks[WAIT])) == 0);
            int current = created;
            pthread_cond_signal(&(conds[WAITING]));
            while (created == current) {
                pthread_cond_wait(&(conds[READY]), &(locks[WAIT]));
            }
            stopping -= 1;
            assert(pthread_mutex_unlock(&(locks[WAIT])) == 0);
        }
        for (int thread = 0; thread < total_threads; ++thread) {
            pthread_join(workers[thread], NULL);
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

    workers = (pthread_t *) calloc(threads, sizeof(pthread_t));
    assert(workers);

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
        Client *c = create_client(connfd);
        if (poll_client(c, polled)) {
            push(polled, c);
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
