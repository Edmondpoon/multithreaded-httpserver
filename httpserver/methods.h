#ifndef _METHODS_H_
#define _METHODS_H_

#include "queue.h"
#include <pthread.h>

extern pthread_mutex_t poll_lock;
enum Status_codes { ERROR, CREATED, OK, NOT_FOUND, POLLED };

// TODO dont need confd
int Get(Client *c, int connfd);

int Put(Client *c, int nonBodyLength, Queue *polled);

int Delete();

int Head();

int Post();

#endif
