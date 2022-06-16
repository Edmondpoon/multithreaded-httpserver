#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BLOCK      4096

// Converts a 64 bit unsigned number to a string and concatenates it with a given string
// returns the concatenated string
char *concatuint64tostr(uint64_t num, char *str) {
    int numbers[1024] = { 0 }, total = 0;
    // Gets all the digits in the number
    while ((double) num / 10 > 0) {
        numbers[total] = num % 10;
        total += 1;
        num /= 10;
    }
    char *concat = (char *) calloc(BLOCK, sizeof(char));
    if (!concat) {
        fprintf(stderr, "Unable to allocate memory.\n");
        return NULL;
    }
    int index = strlen(str);
    memcpy(concat, str, index);

    if (numbers[total - 1] > 0) {
        for (int n = total - 1; n > -1; --n) {
            concat[index++] = numbers[n] + 48;
        }
    } else {
        concat[index++] = 48;
    }
    for (int delimiter = 0; delimiter < 2; ++delimiter) {
        concat[index++] = '\r';
        concat[index++] = '\n';
    }
    return concat;
}

