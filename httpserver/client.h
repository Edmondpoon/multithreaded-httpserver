#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <poll.h>
#include <stdbool.h>

#define UNDEFINED -1

typedef struct Client {
    int id;
    int fd;
    int content_length;
    int method;
    int headers_index;
    int non_body_index;
    char *uri;
    char *headers;
    char tempfile[7];
    bool headers_processed;
    struct pollfd poller;
} Client;

Client *create_client(int fd);

void close_client(Client **c);

void set_id(Client *c, int id);

void set_length(Client *c, int length);

void set_method(Client *c, int method);

#endif
