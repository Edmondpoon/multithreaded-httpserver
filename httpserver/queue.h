#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdbool.h>
#include <poll.h>

typedef struct Queue Queue;

typedef struct connection {
    int id;
    int content_length;
    int fd;
    int method;
    int read;
    char *uri;
    char *tempfile;
    bool request_line;
    bool header;
    struct pollfd poller;
} conn;

conn *create_fd(int fd, int id, int length, int method);

void free_fd(conn **fd);

Queue *create_queue(int capacity);

void free_queue(Queue **q);

void cleanup_init(Queue *q);

bool get_cleanup(Queue *q);

bool empty(Queue *q);

bool full(Queue *q);

int length(Queue *q);

//void push(Queue *q, int connfd, int id, int length, int method, char *uri);
void push(Queue *q, conn *connfd);
//void push(Queue *q, int connfd);

conn *pop(Queue *q);
//int pop(Queue *q);

#endif
