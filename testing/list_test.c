//
// Tests for a linked list implementation
// Any valgrind errors/leaks indicates an issue with the implementation and should be addressed
//
//

#include "../httpserver/ll.h"
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_CYAN    "\x1b[36m"

#define OPTIONS "aesid"
enum Functions { EMPTY, SIZE, ITERATOR, DELETION };

void empty(void) {
    printf(ANSI_COLOR_CYAN "Testing the empty function" ANSI_COLOR_RESET "\n"); 
    printf("---------------------------\n");
    int passed = 0;
    int total = 3;
    List *l = create_list();
    assert(list_empty(l));
    Client *c = create_client(open("Makefile", O_RDONLY));
    list_push(l, c);
    if (!list_empty(l)) {
        printf("Test 1\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n"); 
        passed += 1;
    } else {
        printf("Test 1\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n"); 
    }
    list_push(l, c);
    list_pop(l);
    if (!list_empty(l)) {
        printf("Test 2\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n"); 
        passed += 1;
    } else {
        printf("Test 2\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n"); 
    }
    list_pop(l);
    if (list_empty(l)) {
        printf("Test 3\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n"); 
        passed += 1;
    } else {
        printf("Test 3\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n"); 
    }

    if (passed < total) {
        printf("" ANSI_COLOR_YELLOW "WARNING! Passed %d / %d tests" ANSI_COLOR_RESET "\n", passed, total); 
    } else {
        printf(ANSI_COLOR_GREEN "Passed all empty tests" ANSI_COLOR_RESET "\n"); 
    }
    free_list(&l);
    return;
}

void size(void) {
    printf(ANSI_COLOR_CYAN "Testing the size function" ANSI_COLOR_RESET "\n"); 
    printf("---------------------------\n");
    int passed = 0;
    int total = 4;
    List *l = create_list();
    assert(list_size(l) == 0);
    Client *c = create_client(open("Makefile", O_RDONLY));

    // 1 new entry
    list_push(l, c);
    if (list_size(l) == 1) {
        printf("Test 1\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n"); 
        passed += 1;
    } else {
        printf("Test 1\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n"); 
    }

    // 6 new entries
    list_push(l, c);
    list_push(l, c);
    list_push(l, c);
    list_push(l, c);
    list_push(l, c);
    list_push(l, c);
    assert(list_size(l) == 7);
    if (list_size(l) == 7) {
        printf("Test 2\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n"); 
        passed += 1;
    } else {
        printf("Test 2\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n"); 
    }

    // remove 3 entries
    list_pop(l);
    list_pop(l);
    list_pop(l);
    assert(list_size(l) == 4);
    if (list_size(l) == 4) {
        printf("Test 3\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n"); 
        passed += 1;
    } else {
        printf("Test 3\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n"); 
    }

    // remove 1 entry, add 1 new entry, and remove 2 more entries
    list_pop(l);
    list_push(l, c);
    list_pop(l);
    list_pop(l);
    assert(list_size(l) == 2);
    if (list_size(l) == 2) {
        printf("Test 4\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n"); 
        passed += 1;
    } else {
        printf("Test 4\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n"); 
    }

    // remove the last 2 entries
    list_pop(l);
    list_pop(l);
    assert(list_size(l) == 0);
    if (list_size(l) == 0) {
        printf("Test 5\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n"); 
        passed += 1;
    } else {
        printf("Test 5\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n"); 
    }

    if (passed < total) {
        printf("" ANSI_COLOR_YELLOW "WARNING! Passed %d / %d tests" ANSI_COLOR_RESET "\n", passed, total); 
    } else {
        printf(ANSI_COLOR_GREEN "Passed all size tests" ANSI_COLOR_RESET "\n"); 
    }
    free_list(&l);
    return;
}

void iterator(void) {
    printf(ANSI_COLOR_CYAN "Testing the iterator function" ANSI_COLOR_RESET "\n"); 
    printf("---------------------------\n");
    int passed = 0;
    int total = 14;
    List *l = create_list();
    assert(list_size(l) == 0);
    int fds[7] = {
        open("Makefile", O_RDONLY),
        open("Makefile", O_RDONLY),
        open("Makefile", O_RDONLY),
        open("Makefile", O_RDONLY),
        open("Makefile", O_RDONLY),
        open("Makefile", O_RDONLY),
        open("Makefile", O_RDONLY),
    };

    Client *a = create_client(fds[0]);
    Client *b = create_client(fds[1]);
    Client *c = create_client(fds[2]);
    Client *d = create_client(fds[3]);
    Client *e = create_client(fds[4]);
    Client *f = create_client(fds[5]);
    Client *g = create_client(fds[6]);

    // 7 distinct entries
    list_push(l, a);
    list_push(l, b);
    list_push(l, c);
    list_push(l, d);
    list_push(l, e);
    list_push(l, f);
    list_push(l, g);

    // iterates over the linked list twice
    for (int val = 0; val < 14; ++val) {
        Client *out = list_iterator(l);
        if (out->fd == fds[val % 7]) {
            printf("Test %d\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n", val + 1); 
            passed += 1;
        } else {
            printf("Test %d\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n", val + 1); 
        }
    }    

    if (passed < total) {
        printf("" ANSI_COLOR_YELLOW "WARNING! Passed %d / %d tests" ANSI_COLOR_RESET "\n", passed, total); 
    } else {
        printf(ANSI_COLOR_GREEN "Passed all iterator tests" ANSI_COLOR_RESET "\n"); 
    }
    free_list(&l);
    return;
}

void deletion(void) {
    printf(ANSI_COLOR_CYAN "Testing the deletion function" ANSI_COLOR_RESET "\n"); 
    printf("---------------------------\n");
    int passed = 0;
    int tests = 0;
    List *l = create_list();
    assert(list_size(l) == 0);
    int fds[7] = {
        open("Makefile", O_RDONLY),
        open("Makefile", O_RDONLY),
        open("Makefile", O_RDONLY),
        open("Makefile", O_RDONLY),
        open("Makefile", O_RDONLY),
        open("Makefile", O_RDONLY),
        open("Makefile", O_RDONLY),
    };

    Client *a = create_client(fds[0]);
    Client *b = create_client(fds[1]);
    Client *c = create_client(fds[2]);
    Client *d = create_client(fds[3]);
    Client *e = create_client(fds[4]);
    Client *f = create_client(fds[5]);
    Client *g = create_client(fds[6]);

    // 7 distinct entries
    list_push(l, a);
    list_push(l, b);
    list_push(l, c);
    list_push(l, d);
    list_push(l, e);
    list_push(l, f);
    list_push(l, g);
    print_list(l);

    // deletes client d and f
    for (int val = 0; val < 7; ++val) {
        Client *out = list_iterator(l);
        if (val == 3 || val == 5) {
            delete_cursor(l);
            val += 1;
        }
    }
    printf("-------\n");

    // checks the new list
    int ind = 0;
    for (int val = 0; val < 5; ++val) {
        Client *out = list_iterator(l);
        if (out->fd != fds[3] && out->fd != fds[5]) {
            printf("Test %d\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n", tests + 1); 
            passed += 1;
        } else {
            printf("Test %d\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n", tests + 1); 
        }
        tests += 1;

        if (ind == 3 || ind == 5) {
            ind += 1;
        }
        if (out->fd == fds[ind]) {
            printf("Test %d\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n", tests + 1); 
            passed += 1;
        } else {
            printf("Test %d\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n", tests + 1); 
        }
        tests += 1;
        ind += 1;
    }

    if (passed < tests) {
        printf("" ANSI_COLOR_YELLOW "WARNING! Passed %d / %d tests" ANSI_COLOR_RESET "\n", passed, tests); 
    } else {
        printf(ANSI_COLOR_GREEN "Passed all deletion tests" ANSI_COLOR_RESET "\n"); 
    }
    free_list(&l);
    return;

}


int main(int argc, char *argv[]) {
    int opt = 0;
    void (*tests[4])(void) = { empty, size, iterator, deletion };
    // Flags for the corresponding tests
    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
            case 'a':
                // Tests all tests
                for (int test = 0; test < 4; ++test) {
                    tests[test]();
                    printf("\n\n");
                }
                break;
            case 'e':
                // Test empty function
                tests[EMPTY]();
                printf("\n\n");
                break;
            case 's':
                // Test size function
                tests[SIZE]();
                printf("\n\n");
                break;
            case 'i':
                // Test iterator
                tests[ITERATOR]();
                printf("\n\n");
                break;
            case 'd':
                // Test cursor deletion function
                tests[DELETION]();
                printf("\n\n");
                break;
            default:;
        }
    }
    return EXIT_SUCCESS;
}
