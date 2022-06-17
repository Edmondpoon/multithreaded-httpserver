#ifndef _QUEUE_H_
#define _QUEUE_H_

#include "client.h"

typedef struct Queue Queue;

Queue *create_queue(int capacity);

void free_queue(Queue **q);

void cleanup_init(Queue *q);

bool get_cleanup(Queue *q);

bool empty(Queue *q);

bool full(Queue *q);

int length(Queue *q);

//void push(Queue *q, int connfd, int id, int length, int method, char *uri);
void push(Queue *q, Client *connfd);
//void push(Queue *q, int connfd);

Client *pop(Queue *q);
//int pop(Queue *q);

#endif
