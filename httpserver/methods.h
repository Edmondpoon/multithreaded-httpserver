#ifndef _METHODS_H_
#define _METHODS_H_

#include "queue.h"
#include <pthread.h>

extern pthread_mutex_t poll_lock;
extern Queue *polled;
enum Status_codes { ERROR, CREATED, OK, NOT_FOUND, POLLED };

int Get(conn *c, int connfd);

int Put(conn *c, int connfd, int nonBodyLength);

int Delete();

int Head();

int Post();

#endif

