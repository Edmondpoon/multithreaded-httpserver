#ifndef _PARSER_H_
#define _PARSER_H_

#include <regex.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>

#define REQUEST    "[-a-z0-9.: /_]*\r\n"
#define BLOCK      4096
#define INVALID    -1337
#define INCOMPLETE -420
#define LENGTH     33
#define ID         44
enum Requests { PUT, APPEND, GET };

int regexHeaders(regex_t *regex, char *words[1024], char buffer[2048], int size);

int64_t parseHeaderField(char *header, int *value);

int parseRequestLine(char **uri, char *request);

bool parseUri(char *path, int request);

#endif
