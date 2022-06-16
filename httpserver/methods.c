#include "methods.h"
#include "response.h"
#include "utils.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BLOCK 4096
pthread_mutex_t poll_lock = PTHREAD_MUTEX_INITIALIZER;

int Get(conn *c, int connfd) {
    struct stat sb;
    int exists = stat(c->uri + 1, &sb), directory = S_ISDIR(sb.st_mode);
    if (exists > -1 && !directory) {
        int fd = open(c->uri + 1, O_RDONLY);
        // non blocking TODO
        flock(fd, LOCK_SH);
        if (fd < 0) {
            // broke TODO no file kek
            printf("no file d\n");
            return NOT_FOUND;
        }

        // Get the number of bytes in the file
        stat(c->uri + 1, &sb);
        uint64_t total_read = sb.st_size;
        char *header = concatuint64tostr(total_read, "HTTP/1.1 200 OK\r\nContent-Length: ");
        int header_size = strlen(header);
        int red = read(fd, header + strlen(header), BLOCK - header_size - 1);
        header_size += red;
        total_read -= red;
        write(connfd, header, header_size);
        char buffer[BLOCK] = { 0 };
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
        // TODO log requests in main file
        //logRequest(c->method, c->uri, 200, id);
        return OK;
    }
    return ERROR;
}

int Put(conn *c, int connfd, int nonBodyLength) {
    bool create = false;
    // determines whether the file was created or already existed
    int fd = open(c->uri + 1, O_WRONLY);
    if (fd < 0) {
        fd = open(c->uri + 1, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
        create = true;
    }
    int tempfd = -1;
    if (!c->tempfile) {
        char name[7] = "XXXXXX";
        tempfd = mkstemp(name);
        // TODO free this
        c->tempfile = strdup(name);
    } else {
        tempfd = open(c->tempfile, O_RDWR | O_APPEND);
    }

    int red = 0;
    char buffer[BLOCK] = { 0 };
    if (red - nonBodyLength > -1 && red - nonBodyLength <= c->content_length) {
        write(tempfd, buffer + nonBodyLength, red - nonBodyLength);
        c->content_length -= red - nonBodyLength;
        c->read += red - nonBodyLength;
    } else if (red - nonBodyLength > c->content_length) {
        write(tempfd, buffer + nonBodyLength, c->content_length);
        c->read += c->content_length;
        c->content_length = 0;
    }
    if (c->content_length > 0 && poll(&c->poller, 1, 1500) == 0) {
        pthread_mutex_lock(&poll_lock);
        push(polled, c);
        pthread_mutex_unlock(&poll_lock);
        close(fd);
        close(tempfd);
        return POLLED;
    }
    while (c->content_length > 0) {
        red = read(connfd, buffer, (c->content_length > BLOCK ? BLOCK : c->content_length));
        write(tempfd, buffer, red);
        c->read += red;
        c->content_length -= red;
        if (c->content_length > 0 && poll(&c->poller, 1, 1500) == 0) {
            // TODO also helper func pls

            pthread_mutex_lock(&poll_lock);
            push(polled, c);
            pthread_mutex_unlock(&poll_lock);
            close(tempfd);
            close(fd);
            return POLLED;
        }
    }

    // TODO non blocking
    flock(fd, LOCK_EX);
    rename(c->tempfile, c->uri);
    flock(fd, LOCK_UN);
    close(tempfd);

    //  TODO log in main file
    int status = ERROR;
    if (create) {
        createdResponse(connfd);
        status = CREATED;
    } else {
        okResponse(connfd);
        status = OK;
    }
    return status;
}

int Delete();

int Head();

int Post();

