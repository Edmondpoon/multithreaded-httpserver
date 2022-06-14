#include "../httpserver/parser.h"
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define OPTIONS "hrua"

char *uri_grabber(char *request);

void headers(void) {
    int vTests = 8, iTests = 2;
    char valid_tests[][1024] = { 
        "some key : some value", 
        "key:            value",
        "Content-Length: 32a34",
        "Request-Id: 32a34",
        "Request-Id: 32adasd",
        "Content-Length: 32a",
        "Content-Length:32",
        "Content-Length: 32",
    };
    char invalid_tests[][1024] = {
        "Request-Id: aa",
        "Content-Length: fhd",
    };
    char valid_arg[][1024] = { 
        "some key : some value\r\n", 
        "key:            value\r\n",
        "Content-Length: 32a34\r\n",
        "Request-Id: 32a34\r\n",
        "Request-Id: 32adasd\r\n",
        "Content-Length: 32a\r\n",
        "Content-Length:32\r\n",
        "Content-Length: 32\r\n",
    };
    char invalid_arg[][1024] = {
        "Request-Id: aa\r\n",
        "Content-Length: fhd\r\n",
    };

    // Valid tests
    for (int test = 0; test < vTests; test++) {
        int output = -1, value = 0;
        output = parseHeaderField(valid_arg[test], &value);
        if (output == -1) {
            printf("Test: %s\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n", valid_tests[test]); 
        } else if ((output == ID || output == LENGTH)) {
            if (value == 32) {
                printf("Test: %s\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n", valid_tests[test]); 
            } else {
                printf("Test: %s\t" ANSI_COLOR_YELLOW "WARNING! Got %d, expected 32" ANSI_COLOR_RESET "\n", valid_tests[test], value); 
            }
        } else {
            printf("Test: %s\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n", valid_tests[test]); 
        }
    }
    
    // Invalid tests
    for (int test = 0; test < iTests; test++) {
        int output = -1, value = 0;
        output = parseHeaderField(invalid_arg[test], &value);
        if (output == INVALID) {
            printf("Test: %s\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n", invalid_tests[test]); 
        } else {
            char returned[7] = { 0 };
            switch (output) {
                case ID:
                    memcpy(returned, "ID", 2);
                    break;
                case LENGTH:
                    memcpy(returned, "LENGTH", 6);
                    break;
                case -1:
                    memcpy(returned, "NEG 1", 5);
                    break;
                default:
                    memcpy(returned, "BROKEN", 6);
                    break;
            }
            printf("Test: %s\t" ANSI_COLOR_RED "FAILED! Got %s with value %d, expected INVALID" ANSI_COLOR_RESET "\n", invalid_tests[test], returned, value); 
        }
    }
    return;
}

void requests(void) {
    int vTests = 3, iTests = 9;
    char valid_arg[][1024] = { 
        "PUT /foo.bar HTTP/1.1\r\n",
        "Put / HTTP/1.1\r\n",
        "Put /foo.bar HTTP/1.1   \r\n",
    };

    char invalid_arg[][1024] = {
        "PUt /foo.bar HTTP/1\r\n",
        "PuT /foo.bar HTTP/2.1\r\n",
        "put /foo.bar    HTTP/1.1\r\n",
        "Put    /foo.bar HTTP/1.1\r\n",
        "Put /foo.bar /1.1\r\n",
        "Put / \r\n",
        " / \r\n",
        "\r\n",
        "             \r\n",
    };
    char valid_tests[][1024] = { 
        "PUT /foo.bar HTTP/1.1",
        "Put / HTTP/1.1",
        "Put /foo.bar HTTP/1.1   ",
    };
    char invalid_tests[][1024] = {
        "PUt /foo.bar HTTP/1",
        "PuT /foo.bar HTTP/2.1",
        "put /foo.bar    HTTP/1.1",
        "Put    /foo.bar HTTP/1.1",
        "Put /foo.bar /1.1",
        "Put / ",
        " / ",
        "",
        "             ",
    };

    // Valid tests
    for (int test = 0; test < vTests; test++) {
        int output = -1;
        char *uri = NULL;
        output = parseRequestLine(&uri, valid_arg[test]);
        if (output == INVALID || output == NOT_IMPLEMENTED) {
            printf("Test: %s\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n", valid_tests[test]); 
        } else {
            char *correct = uri_grabber(valid_arg[test]);
            if (output == PUT && !strcmp(uri, correct)) {
                printf("Test: %s\t" ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n", valid_tests[test]); 
            } else {
                printf("Test: %s\t" ANSI_COLOR_YELLOW "WARNING! Got file path %s, expected %s" ANSI_COLOR_RESET "\n", valid_tests[test], uri, correct); 
            }
            free(correct);
        }
    }
    
    // inValid tests
    for (int test = 0; test < iTests; test++) {
        int output = -1;
        char *uri = NULL;
        output = parseRequestLine(&uri, invalid_arg[test]);
        if (output == INVALID || output == NOT_IMPLEMENTED) {
            printf("Test: %s\t" ANSI_COLOR_GREEN "PASSED!" ANSI_COLOR_RESET "\n", invalid_tests[test]); 
        } else {
            printf("Test: %s\t" ANSI_COLOR_RED "FAILED!" ANSI_COLOR_RESET "\n", invalid_tests[test]); 
        }
    }
    return;
}

void uri(void) {
}

int main(int argc, char *argv[]) {
    int opt = 0;
    
    // Flags for the corresponding tests
    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
            case 'a':
                // Test header parser
                headers();
                printf("---------------------------\n");
                requests();
                printf("---------------------------\n");
                uri();
                printf("---------------------------\n");
                break;
            case 'h':
                // Test header parser
                headers();
                break;
            case 'r':
                // Test Request line parser
                requests();
                break;
            case 'u':
                // Test URI parser
                uri();
                break;
            default:;
        }
        printf("---------------------------\n");
    }
    return EXIT_SUCCESS;
}

char *uri_grabber(char *request) {
    int uri_limit = 10;
    char *uri = (char *) calloc(uri_limit, sizeof(char));
    for (int ind = 4; ind < uri_limit + 4 && request[ind] != ' '; ++ind) {
        uri[ind - 4] = request[ind];
    }
    return uri;
}

