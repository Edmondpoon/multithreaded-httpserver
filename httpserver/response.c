#include "response.h"
#include "string.h"

void okResponse(int fd) {
    char *status = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n";
    write(fd, status, strlen(status));
    return;
}

void createdResponse(int fd) {
    char *status = "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n";
    write(fd, status, strlen(status));
    return;
}

void badResponse(int fd) {
    char status[] = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
    write(fd, status, strlen(status));
    return;
}

void forbiddenResponse(int fd) {
    char status[] = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
    write(fd, status, strlen(status));
    return;
}

void notFoundResponse(int fd) {
    char status[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n";
    write(fd, status, strlen(status));
    return;
}

void internalErrorResponse(int fd) {
    char status[]
        = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal Server Error\n";
    write(fd, status, strlen(status));
    return;
}

void notImplementedResponse(int fd) {
    char status[] = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n";
    write(fd, status, strlen(status));
    return;
}
