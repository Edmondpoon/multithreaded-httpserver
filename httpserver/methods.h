#ifndef _METHODS_H_
#define _METHODS_H_

#include "ll.h"
#include "response.h"
#include <pthread.h>

extern pthread_mutex_t poll_lock;

int Get(Client *c);

int Put(Client *c, List *polled);

int Head(Client *c);

int Options(Client *c);

int Append(Client *c, List *polled);

#endif
