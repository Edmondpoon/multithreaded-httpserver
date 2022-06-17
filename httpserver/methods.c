#include "methods.h"
#include "response.h"
#include "utils.h"
#include <err.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BLOCK 4096
pthread_mutex_t poll_lock = PTHREAD_MUTEX_INITIALIZER;

int Get(Client *connection, int connfd) {
    struct stat sb;
    int exists = stat(connection->uri + 1, &sb), directory = S_ISDIR(sb.st_mode);
    if (exists > -1 && !directory) {
        int fd = open(connection->uri + 1, O_RDONLY);
        if (fd < 0) {
            return NOT_FOUND;
        }
        // non blocking TODO
        flock(fd, LOCK_SH);
        // Get the number of bytes in the file
        stat(connection->uri + 1, &sb);
        uint64_t total_read = sb.st_size;
        char *header = concat_str(total_read, "HTTP/1.1 200 OK\r\nContent-Length: ");
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
            int index = 0;
            while (red > 0) {
                int written = write(connfd, buffer + index, red);
                index += written;
                red -= written;
            }
        }
        flock(fd, LOCK_UN);
        free(header);
        close(fd);
        return OK;
    }
    return ERROR;
}

int Put(Client *connection, int nonBodyLength, Queue *polled) {
    bool create = false;
    // determines whether the file was created or already existed
    int fd = open(connection->uri + 1, O_WRONLY);
    if (fd < 0) {
        fd = open(connection->uri + 1, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
        create = true;
    }
    int tempfd = -1;
    if (!strcmp(connection->tempfile, "XXXXXX")) {
        tempfd = mkstemp(connection->tempfile);
    } else {
        tempfd = open(connection->tempfile, O_RDWR | O_APPEND);
    }
    if (tempfd < 0) {
        printf("broke\n");
        // TODO
        return ERROR;
    }

    char buffer[BLOCK + 1] = { 0 };
    int red = connection->headers_index;
    if (red - nonBodyLength > -1 && red - nonBodyLength <= connection->content_length) {
        write(tempfd, connection->headers + nonBodyLength, red - nonBodyLength);
        connection->content_length -= red - nonBodyLength;
    } else if (red - nonBodyLength > connection->content_length) {
        write(tempfd, connection->headers + nonBodyLength, connection->content_length);
        connection->content_length = 0;
    }
    if (connection->content_length > 0 && poll_client(connection, polled)) {
        return POLLED;
    }
    while (connection->content_length > 0) {
        red = read(connection->fd, buffer,
            (connection->content_length > BLOCK ? BLOCK : connection->content_length));
        write(tempfd, buffer, red);
        connection->content_length -= red;
        if (connection->content_length > 0 && poll_client(connection, polled)) {
            return POLLED;
        }
    }

    // TODO non blocking
    flock(fd, LOCK_EX);
    rename(connection->tempfile, connection->uri + 1);
    flock(fd, LOCK_UN);
    close(tempfd);
    close(fd);

    int status = ERROR;
    if (create) {
        createdResponse(connection->fd);
        status = CREATED;
    } else {
        okResponse(connection->fd);
        status = OK;
    }
    return status;
}

int Delete();

int Head();

int Post();
