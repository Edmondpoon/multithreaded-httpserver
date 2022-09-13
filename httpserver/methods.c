#include "methods.h"
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

int Get(Client *connection) {
    int connfd = connection->fd;
    struct stat sb;
    stat(connection->uri + 1, &sb);
    int directory = S_ISDIR(sb.st_mode);
    if (!directory) {
        int fd = open(connection->uri + 1, O_RDONLY);
        if (fd < 0) {
            return NOT_FOUND;
        }
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
    } else {
        forbidden_response(connfd);
        return FORBIDDEN;
    }
}

int Put(Client *connection, List *polled) {
    int connfd = connection->fd;
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
        return INTERNAL_ERROR;
    }

    char buffer[BLOCK + 1] = { 0 };
    int red = connection->headers_index;
    int nonBodyLength = connection->non_body_index;
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
        red = read(connfd, buffer,
            (connection->content_length > BLOCK ? BLOCK : connection->content_length));
        write(tempfd, buffer, red);
        connection->content_length -= red;
        if (connection->content_length > 0 && poll_client(connection, polled)) {
            return POLLED;
        }
    }

    int status = ERROR;
    flock(fd, LOCK_EX);
    rename(connection->tempfile, connection->uri + 1);
    if (create) {
        created_response(connfd);
        status = CREATED;
    } else {
        ok_response(connfd);
        status = OK;
    }
    flock(fd, LOCK_UN);
    close(tempfd);
    close(fd);

    return status;
}

int Head(Client *connection) {
    int connfd = connection->fd;
    struct stat sb;
    int directory = S_ISDIR(sb.st_mode);
    if (!directory) {
        int fd = open(connection->uri + 1, O_RDONLY);
        if (fd < 0) {
            return NOT_FOUND;
        }
        flock(fd, LOCK_SH);
        // Get the number of bytes in the file
        stat(connection->uri + 1, &sb);
        uint64_t total_read = sb.st_size;
        char *header = concat_str(total_read, "HTTP/1.1 200 OK\r\nContent-Length: ");
        int header_size = strlen(header);
        write(connfd, header, header_size);
        flock(fd, LOCK_UN);
        free(header);
        close(fd);
        return OK;
    } else {
        forbidden_response(connfd);
        return FORBIDDEN;
    }
}

int Options(Client *connection) {
    char status[] = "HTTP/1.1 204 No Content\r\nAllow: GET,HEAD,PUT,OPTIONS\r\n\r\n";
    write(connection->fd, status, strlen(status));
    return OK;
}

int Append(Client *connection, List *polled) {
    int connfd = connection->fd;
    struct stat sb;
    stat(connection->uri + 1, &sb);
    int directory = S_ISDIR(sb.st_mode);
    int fd = open(connection->uri + 1, O_APPEND | O_WRONLY);
    if (!directory || fd < 0) {
        // File doesn't exist
        if (fd < 0) {
            return NOT_FOUND;
        }
    }
    int tempfd = -1;
    if (!strcmp(connection->tempfile, "XXXXXX")) {
        tempfd = mkstemp(connection->tempfile);
    } else {
        tempfd = open(connection->tempfile, O_RDWR | O_APPEND);
    }
    if (tempfd < 0) {
        return INTERNAL_ERROR;
    }

    char buffer[BLOCK + 1] = { 0 };
    int red = connection->headers_index;
    int nonBodyLength = connection->non_body_index;
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
        red = read(connfd, buffer,
            (connection->content_length > BLOCK ? BLOCK : connection->content_length));
        write(tempfd, buffer, red);
        connection->content_length -= red;
        if (connection->content_length > 0 && poll_client(connection, polled)) {
            return POLLED;
        }
    }

    flock(fd, LOCK_EX);
    // Reset read head of temp file to start
    lseek(tempfd, 0, SEEK_SET);
    while ((red = read(tempfd, buffer, BLOCK - 1)) > 0) {
        int ind = 0;
        while (red > 0) {
            int written = write(fd, buffer + ind, red);
            ind += written;
            red -= written;
        }
    }
    ok_response(connfd);
    flock(fd, LOCK_UN);
    close(tempfd);
    close(fd);

    return OK;
}
