#ifndef _RESPONSE_H_
#define _RESPONSE_H_

#include <unistd.h>

#define OK 200
#define CREATED 201
#define NO_CONTENT 204
#define FORBIDDEN 403
#define NOT_FOUND 404
#define INTERNAL_ERROR 500

// TODO define for the actual values rather than indices if not used as indices
enum Misc_codes { ERROR, POLLED };

void ok_response(int fd);

void created_response(int fd);

void bad_response(int fd);

void notFound_response(int fd);

void internalError_response(int fd);

void forbidden_response(int fd);

void notImplemented_response(int fd);

#endif

