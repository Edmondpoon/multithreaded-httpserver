#ifndef _UTILS_H_
#define _UTILS_H_

#include "queue.h"
#include <inttypes.h>

char *concat_str(uint64_t num, char *str);

bool poll_client(Client *client, Queue *polled);

#endif
