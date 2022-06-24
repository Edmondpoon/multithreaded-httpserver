#include "client.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdio.h>

#define DEFAULT_ID  0
#define HEADER_SIZE 2048

Client *create_client(int fd) {
    Client *c = (Client *) calloc(1, sizeof(Client));
    if (c) {
        c->id = DEFAULT_ID;
        c->content_length = UNDEFINED;
        c->fd = fd;
        c->method = UNDEFINED;
        memcpy(c->tempfile, "XXXXXX", 7);
        c->headers = (char *) calloc(HEADER_SIZE + 1, sizeof(char));
        c->headers_processed = false;
        c->headers_index = 0;
        c->non_body_index = 0;
        c->uri = NULL;
        c->poller.fd = fd;
        c->poller.events = POLLIN;
        c->poller.revents = POLLIN;
    }
    return c;
}

void close_client(Client **c) {
    if (*c) {
        if ((*c)->uri) {
            free((*c)->uri);
        }
        free((*c)->headers);
        close((*c)->fd);
        free(*c);
        *c = NULL;
    }
    return;
}

void set_id(Client *c, int id) {
    c->id = id;
    return;
}

void set_length(Client *c, int length) {
    c->content_length = length;
    return;
}

void set_method(Client *c, int method) {
    c->method = method;
    return;
}
