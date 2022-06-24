#ifndef _LL_H_
#define _LL_H_

#include "client.h"

typedef struct LinkedList List;

List *create_list(void);

void free_list(List **l);

bool list_empty(List *l);

int list_size(List *l);

void list_push(List *l, Client *c);

Client *list_pop(List *l);

// basically called in loop in main file (struct has cursor element) and this func keeps "yielding"
Client *list_iterator(List *l);

// TODO
// pops the cursor
void delete_cursor(List *l);

void print_list(List *l);

#endif
