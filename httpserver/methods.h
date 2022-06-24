#ifndef _METHODS_H_
#define _METHODS_H_

#include "ll.h"
#include <pthread.h>

extern pthread_mutex_t poll_lock;
enum Status_codes { ERROR, CREATED, OK, NOT_FOUND, POLLED };

int Get(Client *c);

int Put(Client *c, List *polled);

int Delete();

int Head();

int Post();

#endif
