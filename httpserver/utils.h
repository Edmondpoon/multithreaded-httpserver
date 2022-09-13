#ifndef _UTILS_H_
#define _UTILS_H_

#include "ll.h"
#include <inttypes.h>

char *concat_str(uint64_t num, char *str);

bool poll_client(Client *client, List *polled);

int write_bytes(int fd, char *buffer, int nbytes);

#endif
