#ifndef _RESPONSE_H_
#define _RESPONSE_H_

#include <unistd.h>

void okResponse(int fd);

void createdResponse(int fd);

void notFoundResponse(int fd);

void internalErrorResponse(int fd);

#endif
