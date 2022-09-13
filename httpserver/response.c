#include "response.h"
#include "string.h"

void ok_response(int fd) {
    char *status = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n";
    write(fd, status, strlen(status));
    return;
}

void created_response(int fd) {
    char *status = "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n";
    write(fd, status, strlen(status));
    return;
}

void bad_response(int fd) {
    char status[] = "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n";
    write(fd, status, strlen(status));
    return;
}

void forbidden_response(int fd) {
    char status[] = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n";
    write(fd, status, strlen(status));
    return;
}

void notFound_response(int fd) {
    char status[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n";
    write(fd, status, strlen(status));
    return;
}

void internalError_response(int fd) {
    char status[]
        = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal Server Error\n";
    write(fd, status, strlen(status));
    return;
}

void notImplemented_response(int fd) {
    char status[] = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n";
    write(fd, status, strlen(status));
    return;
}


