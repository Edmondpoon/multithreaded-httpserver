#include "queue.h"
#include <stdlib.h>
#include <string.h>

struct Queue {
    int front;
    int tail;
    int length;
    int cap;
    Client **queue;
    bool cleanup;
};

Queue *create_queue(int capacity) {
    Queue *q = (Queue *) calloc(1, sizeof(Queue));
    if (q) {
        q->front = 0;
        q->tail = 0;
        q->length = 0;
        q->cap = capacity;
        q->cleanup = false;
        q->queue = (Client **) calloc(capacity, sizeof(Client *));
    }
    return q;
}

void free_queue(Queue **q) {
    if (*q) {
        while (!queue_empty(*q)) {
            Client *temp = queue_pop(*q);
            close_client(&temp);
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

bool queue_empty(Queue *q) {
    return q->length == 0;
}

bool queue_full(Queue *q) {
    return q->length == q->cap;
}

int queue_length(Queue *q) {
    return q->length;
}

void queue_push(Queue *q, Client *connfd) {
    if (!queue_full(q)) {
        q->queue[q->tail] = connfd;
        q->tail = (q->tail + 1) % q->cap;
        q->length += 1;
    }
    return;
}

Client *queue_pop(Queue *q) {
    if (queue_empty(q)) {
        return NULL;
    }
    Client *fd = q->queue[q->front];
    q->front = (q->front + 1) % q->cap;
    q->length -= 1;
    return fd;
}
