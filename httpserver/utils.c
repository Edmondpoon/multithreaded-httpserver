#include "utils.h"
#include "methods.h"
#include <stdlib.h>
#include <assert.h>
#include <sys/poll.h>
#include <string.h>

#define BLOCK   4096
#define TIMEOUT 1500

// Converts a 64 bit unsigned number to a string and concatenates it with a given string
// returns the concatenated string
char *concat_str(uint64_t num, char *str) {
    int numbers[1024] = { 0 }, total = 0;
    // Gets all the digits in the number
    while ((double) num / 10 > 0) {
        numbers[total] = num % 10;
        total += 1;
        num /= 10;
    }
    char *concat = (char *) calloc(BLOCK, sizeof(char));
    assert(concat);
    int index = strlen(str);
    memcpy(concat, str, index);

    if (numbers[total - 1] > 0) {
        for (int n = total - 1; n > -1; --n) {
            concat[index++] = numbers[n] + '0';
        }
    } else {
        concat[index++] = '0';
    }
    for (int delimiter = 0; delimiter < 2; ++delimiter) {
        concat[index++] = '\r';
        concat[index++] = '\n';
    }
    return concat;
}

bool poll_client(Client *client, Queue *polled) {
    if (poll(&client->poller, 1, TIMEOUT) == 0) {
        assert(pthread_mutex_lock(&poll_lock) == 0);
        push(polled, client);
        assert(pthread_mutex_unlock(&poll_lock) == 0);
        return true;
    }
    return false;
}
