#include "queue.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct Queue {
    int front;
    int tail;
    int length;
    int cap;
    conn **queue;
    bool cleanup;
};

conn *create_fd(int fd, int id, int length, int method) {
    conn *c = (conn *) calloc(1, sizeof(conn));
    if (c) {
        c->id = id;
        c->content_length = length;
        c->fd = fd;
        c->method = method;
        c->read = 0;
        c->body = NULL;
        c->request_line = false;
        c->header = false;
        c->uri = NULL;
        c->poller.fd = fd;
        c->poller.events = POLLIN;
        c->poller.revents = POLLIN;
    }
    return c;
}

void free_fd(conn **fd) {
    if (*fd) {
        if ((*fd)->uri) {
            free((*fd)->uri);
        }
        if ((*fd)->body) {
            free((*fd)->body);
        }
        close((*fd)->fd);
        free(*fd);
        *fd = NULL;
    }
    return;
}

Queue *create_queue(int capacity) {
    Queue *q = (Queue *) calloc(1, sizeof(Queue));
    if (q) {
        q->front = 0;
        q->tail = 0;
        q->length = 0;
        q->cap = capacity;
        q->cleanup = false;
        q->queue = (conn **) calloc(capacity, sizeof(conn *));
        //q->queue = (int *) calloc(capacity, sizeof(int));
        // TODO check
    }
    return q;
}

void free_queue(Queue **q) {
    if (*q) {
        while (!empty(*q)) {
            conn *temp = pop(*q);
            free_fd(&temp);
        }
        free((*q)->queue);
        free(*q);
        *q = NULL;
    }
    return;
}

void cleanup_init(Queue *q) {
    q->cleanup = true;
    return;
}

bool get_cleanup(Queue *q) {
    return q->cleanup;
}

bool empty(Queue *q) {
    return q->length == 0;
}

bool full(Queue *q) {
    return q->length == q->cap;
}

int length(Queue *q) {
    return q->length;
}

void push(Queue *q, conn *connfd) {
    if (!full(q)) {
        q->queue[q->tail] = connfd;
        q->tail = (q->tail + 1) % q->cap;
        q->length += 1;
    }
    return;
}

conn *pop(Queue *q) {
    if (empty(q)) {
        return NULL;
    }
    conn *fd = q->queue[q->front];
    q->front = (q->front + 1) % q->cap;
    q->length -= 1;
    return fd;
}
