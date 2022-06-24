#include "ll.h"
#include <stdlib.h>

#include <stdio.h>

typedef struct Node Node;

struct Node {
    Node *next;
    Node *prev;
    Client *client;
};

struct LinkedList {
    Node *fDummy;
    Node *bDummy;
    Node *head;
    Node *tail;
    Node *cursor;
    int length;
};

Node *create_node(Client *c) {
    Node *n = (Node *) calloc(1, sizeof(Node));
    if (n) {
        n->client = c;
        n->next = NULL;
        n->prev = NULL;
    }
    return n;
}

Client *free_node(Node **n) {
    Client *client = NULL;
    if (*n) {
        client = (*n)->client;
        free(*n);
        *n = NULL;
    }
    return client;
}

List *create_list(void) {
    List *l = (List *) calloc(1, sizeof(List));
    if (l) {
        l->fDummy = create_node(NULL);
        l->bDummy = create_node(NULL);
        l->fDummy->next = l->bDummy;
        l->bDummy->prev = l->fDummy;
        l->head = NULL;
        l->tail = NULL;
        l->cursor = NULL;
        l->length = 0;
    }
    return l;
}

void free_list(List **l) {
    if (*l) {
        while (!list_empty(*l)) {
            Client *temp = list_pop(*l);
            close_client(&temp);
        }
        Client *temp = free_node(&(*l)->fDummy);
        temp = free_node(&(*l)->bDummy);
        free(*l);
        *l = NULL;
    }
    return;
}

bool list_empty(List *l) {
    return l->length == 0;
}

int list_size(List *l) {
    return l->length;
}

void list_push(List *l, Client *c) {
    Node *n = create_node(c);
    if (!n) {
        return;
    } else if (list_empty(l)) {
        l->head = n;
        n->prev = l->fDummy;
    } else {
        l->tail->next = n;
        n->prev = l->tail;
    }
    n->next = l->bDummy;
    l->tail = n;
    l->length += 1;
    return;
}

Client *list_pop(List *l) {
    if (list_empty(l)) {
        return NULL;
    } else {
        Node *temp = l->tail;
        l->tail = l->tail->prev;
        l->tail->next = l->bDummy;
        Client *c = free_node(&temp);
        l->length -= 1;
        return c;
    }
}

// basically called in loop in main file (struct has cursor element) and this func keeps "yielding"
Client *list_iterator(List *l) {
    if (list_empty(l)) {
        return NULL;
    } else if (!l->cursor || !l->cursor->next->client) {
        l->cursor = l->head;
    } else {
        l->cursor = l->cursor->next;
    }
    Client *next = l->cursor->client;
    return next;
}

// pops the cursor
void delete_cursor(List *l) {
    if (!list_empty(l) && l->cursor) {
        Node *temp = l->cursor;
        l->cursor->prev->next = l->cursor->next;
        l->cursor->next->prev = l->cursor->prev;
        if (!l->cursor->next->client) {
            l->cursor = NULL;
        } else {
            l->cursor = l->cursor->prev;
        }
        if (l->head == temp && list_size(l) - 1 > 0) {
            l->head = temp->next;
        }
        if (l->tail == temp && list_size(l) - 1 > 0) {
            l->tail = temp->prev;
        }
        free_node(&temp);
        l->length -= 1;
    }
    return;
}

void print_list(List *l) {
    if (list_empty(l)) {
        return;
    }
    Node *current = l->head;
    while (current && current->client) {
        printf("%d", current->client->fd);
        current = current->next;
        if (current && current->client) {
            printf(" -> ");
        }
    }
    printf("\n");
    return;
}
