// TODO use epoll
#include "queue.h"
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
#define BUF_SIZE             4096
#define REQUEST_MAX          2048
#define DEFAULT_THREAD_COUNT 4
#define HASH_SIZE            1 << 16
static FILE *logfile;
#define LOG(...) fprintf(logfile, __VA_ARGS__);
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wait_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t waiting = PTHREAD_COND_INITIALIZER;
pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
pthread_mutex_t fill_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t filled = PTHREAD_COND_INITIALIZER;
pthread_mutex_t poll_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t hash_lock = PTHREAD_MUTEX_INITIALIZER;
// working is number of threads currently working
// created is the number of threads that haven't been destroyed (used to ensure all threads close when sigterm)
int working = 0, created = 0;
//int created = 0;
Queue *queue = NULL;
Queue *polled = NULL;
//pthread_t *workers = NULL;

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
    pthread_mutex_lock(&log_lock);
    LOG("%s,%s,%d,%d\n", req, path, status, id);
    fflush(logfile);
    pthread_mutex_unlock(&log_lock);
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

// Converts a 64 bit unsigned number to a string and concatenates it with a given string
// returns the concatenated string
char *concatuint64tostr(uint64_t num, char *str) {
    int numbers[1024] = { 0 }, total = 0;
    // Gets all the digits in the number
    while ((double) num / 10 > 0) {
        numbers[total] = num % 10;
        total += 1;
        num /= 10;
    }
    char *concat = (char *) calloc(BLOCK, sizeof(char));
    if (!concat) {
        fprintf(stderr, "Unable to allocate memory.\n");
        return NULL;
    }
    int index = strlen(str);
    memcpy(concat, str, index);

    if (numbers[total - 1] > 0) {
        for (int n = total - 1; n > -1; --n) {
            concat[index++] = numbers[n] + 48;
        }
    } else {
        concat[index++] = 48;
    }
    for (int delimiter = 0; delimiter < 2; ++delimiter) {
        concat[index++] = '\r';
        concat[index++] = '\n';
    }
    return concat;
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
    // TODO error hamdlinh
    /*
    int flags = fcntl(listenfd, F_GETFL);
    fcntl(listenfd, F_SETFL, flags | O_NONBLOCK);
    */
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
    char buffer[BLOCK] = { 0 };
    char *parsed[1024];
    int fd = -1;
    int nonBodyLength = 0, red = 0;
    int connfd = c->fd, matches = 0;
    //printf("requestline\n");
    if (!c->request_line) {
        red = read(connfd, buffer, REQUEST_MAX);
        matches = regexHeaders(&reg, parsed, buffer, strlen(buffer));
        if (matches < 1) {
            fprintf(stderr, "no valid regex match\n");
            freeRegex(parsed, matches);
            return;
        }
        // Helps keep track of whether the buffer includes some body text
        nonBodyLength += strlen(parsed[0]);
        c->method = parseRequestLine(&(c->uri), parsed[0]);
        c->request_line = true;
        //printf("request line %s\n", buffer);
    }
    bool full_header = false;
    for (int i = 0; i < red - 3; ++i) {
        // Checks to see if we have encountered every header in the request
        if (buffer[i] == '\r' && buffer[i + 1] == '\n' && buffer[i + 2] == '\r'
            && buffer[i + 3] == '\n') {
            full_header = true;
        }
    }

    int id = c->id;
    for (int match = 1; match < matches; ++match) {
        int value = 0;
        int l = strlen(parsed[match]);
        //printf(" parsing %s\n", parsed[match]);
        nonBodyLength += l;
        if (l == 2 && parsed[match][0] == '\r' && parsed[match][1] == '\n') {
            // Found the empty header
            c->header = true;
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
    //printf("header\n");
    if (!full_header && !c->header) {
        bool body = false;
        int temp = red;
        if (poll(&c->poller, 1, 1500) == 0) {
            /*
            if (full(polled)) {
                printf("full\n");
            }
            */
            //printf("polled\n");
            pthread_mutex_lock(&poll_lock);
            push(polled, c);
            pthread_mutex_unlock(&poll_lock);
            return;
        }
        //printf(" not polled\n");
        int extras = 0;
        while ((red = read(connfd, buffer + extras, REQUEST_MAX - temp)) > 0) {
            //printf("reading\n");
            temp += red;
            extras += red;
            if (strstr(buffer, "\r\n\r\n")) {
                body = true;
                break;
            }
        }
        if (!body) {
            fprintf(stderr, "invalid request (no body)\n");
            regfree(&reg);
            return;
        }
        buffer[extras] = '\0';
    }

    if (!c->header) {
        matches = regexHeaders(&reg, parsed, buffer, strlen(buffer));
        if (matches < 1) {
            fprintf(stderr, "no valid regex match\n");
            freeRegex(parsed, matches);
            return;
        }
        // parse headers
        // Start at after the first pair of \r\n of the request line
        // Helps keep track of whether the buffer includes some body text
        for (int match = 0; match < matches; ++match) {
            int value = 0;
            int l = strlen(parsed[match]);
            //printf("header %s\n", parsed[match]);
            nonBodyLength += l;
            if (l == 2 && parsed[match][0] == '\r' && parsed[match][1] == '\n') {
                // Found the empty header
                c->header = true;
                //printf("breaking\n");
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
        //printf("short uri\n");
        logRequest(c->method, c->uri, 404, c->id);
        free_fd(&c);
        notFoundResponse(connfd);
        return;
    } else if (!c->tempfile && !parseUri(c->uri, c->method)) {
        // parse URI to deal with directories
        //printf("invalid uri\n");
        logRequest(c->method, c->uri, 500, c->id);
        free_fd(&c);
        internalErrorResponse(connfd);
        return;
    }

    struct stat sb;
    int exists = stat(c->uri + 1, &sb), directory = S_ISDIR(sb.st_mode);

    if (exists < 0 && c->method != PUT) {
        //printf("not exist\n");
        logRequest(c->method, c->uri, 404, id);
        notFoundResponse(connfd);
    } else if (c->method == GET) {
        if (exists > -1 && !directory) {

            fd = open(c->uri + 1, O_RDONLY);
            // non blocking TODO
            flock(fd, LOCK_SH);
            if (fd < 0) {
                // broke TODO no file kek
                printf("no file d\n");
                return;
            }

            // Get the number of bytes in the file

            stat(c->uri + 1, &sb);
            uint64_t total_read = sb.st_size;
            char *header = concatuint64tostr(total_read, "HTTP/1.1 200 OK\r\nContent-Length: ");
            int header_size = strlen(header);
            red = read(fd, header + strlen(header), BLOCK - header_size - 1);
            header_size += red;
            total_read -= red;
            write(connfd, header, header_size);
            while (total_read > 0) {
                red = read(fd, buffer, BLOCK - 1);
                total_read -= red;
                // TODO
                int f = 0;
                while (red > 0) {
                    int written = write(connfd, buffer + f, red);
                    f += written;
                    red -= written;
                }
            }
            flock(fd, LOCK_UN);
            free(header);
            logRequest(c->method, c->uri, 200, id);
        }
    } else if (c->method == PUT || c->method == APPEND) {
        bool create = false;
        if (c->method == PUT) {
            // determines whether the file was created or already existed
            fd = open(c->uri + 1, O_WRONLY);
            if (fd < 0) {
                fd = open(c->uri + 1, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
                create = true;
            }
        } else if (c->method == APPEND) {
            fd = open(c->uri + 1, O_APPEND | O_WRONLY);
        }
        if (fd > -1 || c->method != APPEND) {
            int tempfd = -1;
            if (!c->tempfile) {
                char name[7] = "XXXXXX";
                tempfd = mkstemp(name);
                // TODO free this
                c->tempfile = strdup(name);
            } else {
                tempfd = open(c->tempfile, O_RDWR | O_APPEND);
            }

            if (red - nonBodyLength > -1 && red - nonBodyLength <= c->content_length) {
                //write(fd, buffer + nonBodyLength, red - nonBodyLength);
                write(tempfd, buffer + nonBodyLength, red - nonBodyLength);
                /*
                for (int chr = 0; chr < red - nonBodyLength; ++chr) {
                    c->body[c->read + chr] = buffer[nonBodyLength + chr];
                
                }
                */
                //memcpy(c->body + c->read, buffer + nonBodyLength, red - nonBodyLength);
                //printf("hered%d\t%d\t%d\n", red,  nonBodyLength, c->content_length);
                c->content_length -= red - nonBodyLength;
                c->read += red - nonBodyLength;
            } else if (red - nonBodyLength > c->content_length) {
                write(tempfd, buffer + nonBodyLength, c->content_length);
                /*
                for (int chr = 0; chr < c->content_length; ++chr) {
                    c->body[c->read] = buffer[nonBodyLength + chr];
                }
                */
                //memcpy(c->body + c->read, buffer + nonBodyLength, c->content_length);
                c->read += c->content_length;
                c->content_length = 0;
                //write(fd, buffer + nonBodyLength, c->content_length);
                //printf("there\n");
            }
            if (c->content_length > 0 && poll(&c->poller, 1, 1500) == 0) {
                /*
                if (full(polled)) {
                    printf("full\n");
                }
                */
                pthread_mutex_lock(&poll_lock);
                push(polled, c);
                pthread_mutex_unlock(&poll_lock);
                close(fd);
                close(tempfd);
                return;
            }
            //printf("filled\n");
            //printf("before red%d\t%d\n", c->content_length, red);
            //bool full = true;
            while (c->content_length > 0) {
                red = read(connfd, buffer, (c->content_length > BLOCK ? BLOCK : c->content_length));
                //printf("reading\n");
                write(tempfd, buffer, red);
                c->read += red;
                // write(fd, buffer, red);
                c->content_length -= red;
                if (c->content_length > 0 && poll(&c->poller, 1, 1500) == 0) {
                    // TODO also helper func pls

                    //printf("stalled\n");
                    /*
                    if (full(polled)) {
                        printf("full\n");
                    }
                    */
                    pthread_mutex_lock(&poll_lock);
                    push(polled, c);
                    pthread_mutex_unlock(&poll_lock);
                    close(tempfd);
                    close(fd);
                    return;
                }
            }

            // TODO non blocking
            flock(fd, LOCK_EX);
            rename(c->tempfile, c->uri);
            /*
            if (c->method == PUT) {
                ftruncate(fd, 0);
            }
            int ind = 0;
            while (c->read > 0) {
                int written = write(fd, c->body + ind, (c->read > BLOCK ? BLOCK : c->read));
                ind += written;
                c->read -= written;
            }
            */
            flock(fd, LOCK_UN);
            close(tempfd);

            if (create && c->method == PUT) {
                logRequest(c->method, c->uri, 201, id);
                createdResponse(connfd);
            } else {
                logRequest(c->method, c->uri, 200, id);
                okResponse(connfd);
            }
        }
    }
    //printf("d%d\n", c->id);
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
    if (pthread_mutex_lock(&wait_lock) != 0) {
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
            if (pthread_cond_timedwait(&waiting, &wait_lock, &ts) == ETIMEDOUT && !empty(polled)) {
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
                //}
                /*
                if (!empty(readers)) {
                    for (int i = 0; i < length(readers); ++i) {
                        // TODO assumes doesnt get full
                        conn *temp = pop(readers);
                        if (!temp->node->writing) {
                            push(queue, temp);
                        } else {
                            push(readers, temp);
                        }
                    }
                }
                if (!empty(writers)) {
                    for (int i = 0; i < length(writers); ++i) {
                        // TODO assumes doesnt get full
                        conn *temp = pop(writers);
                        if (!temp->node->writing && temp->node->readers < 1) {
                            push(queue, temp);
                        } else {
                            push(writers, temp);
                        }
                    }

                }
                */
                pthread_mutex_unlock(&poll_lock);
            }
        }
        if (get_cleanup(queue)) {
            break;
        }
        /*
        if (length(polled) > 0 || length(queue) > 1) {
            pthread_cond_broadcast(&waiting);
        }
        */
        conn *client = pop(queue);
        pthread_cond_signal(&filled);
        working += 1;
        if (pthread_mutex_unlock(&wait_lock) != 0) {
            fprintf(stderr, "Failed to unlock mutex.\n");
            return NULL;
        }

        handle_connection(client, reg);
        if (pthread_mutex_lock(&wait_lock) != 0) {
            fprintf(stderr, "Failed to lock mutex.\n");
            return NULL;
        }
        working -= 1;
    }
    created -= 1;
    pthread_cond_signal(&ready);
    if (pthread_mutex_unlock(&wait_lock) != 0) {
        fprintf(stderr, "Failed to unlock mutex.\n");
        return NULL;
    }
    regfree(&reg);
    return NULL;
}

static void sigterm_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        if (pthread_mutex_lock(&wait_lock) != 0) {
            warn("Failed to lock mutex.\n");
        }
        cleanup_init(queue);
        int stopping = created;
        if (pthread_mutex_unlock(&wait_lock) != 0) {
            warn("Failed to unlock mutex.\n");
        }
        int total = created;
        while (stopping > 0) {
            // signals to every thread that we no longer need them
            // has the threads end their loops
            if (pthread_mutex_lock(&wait_lock) != 0) {
                warn("Failed to lock mutex.\n");
            }
            int current = created;
            pthread_cond_signal(&waiting);
            while (created == current) {
                pthread_cond_wait(&ready, &wait_lock);
            }
            stopping -= 1;
            if (pthread_mutex_unlock(&wait_lock) != 0) {
                warn("Failed to unlock mutex.\n");
            }
        }
        if (queue) {
            free_queue(&queue);
        }
        if (polled) {
            free_queue(&polled);
        }
        /*
        for (int i = 0; i < total; ++i) {
            if (workers[i] && pthread_join(workers[i], NULL) != 0) {
                warnx("Failed to join a thread.\n");
                exit(EXIT_FAILURE);
            }
        }
        free(workers);
        */
        pthread_mutex_destroy(&wait_lock);
        pthread_mutex_destroy(&log_lock);
        pthread_cond_destroy(&waiting);
        pthread_cond_destroy(&ready);
        pthread_cond_destroy(&filled);
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

    //   workers = (pthread_t *) calloc(threads, sizeof(pthread_t));
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
        assert(pthread_mutex_lock(&wait_lock) != 0);
        while (full(queue)) {
            // Full bounded queue so the current client has to be put on hold
            pthread_cond_wait(&filled, &wait_lock);
        }
        conn *c = create_fd(connfd, 0, 0, -1);
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
            pthread_cond_signal(&waiting);
        }

        assert(pthread_mutex_unlock(&wait_lock) != 0);
    }
    return EXIT_SUCCESS;
}
