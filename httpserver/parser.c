#include "parser.h"
#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include <stdio.h>

// Parses a buffer for phrases that match a given regex
int regex_headers(regex_t *regex, char *words[1024], char buffer[2048], int size) {
    regmatch_t match;
    int matches = 0;
    char *temp = buffer;
    for (int i = 0; i < size; ++i) {
        if (regexec(regex, temp, 1, &match, 0)) {
            break;
        } else if (match.rm_so < 0) {
            break;
        }
        uint32_t start = (uint32_t) match.rm_so;
        uint32_t end = (uint32_t) match.rm_eo;
        uint32_t length = end - start;
        words[matches] = (char *) calloc(length + 1, sizeof(char));
        memcpy(words[matches], temp + start, length);
        matches += 1;
        temp += end;
    }
    return matches;
}

// Parses a header field for the key and value
// returns whether the header is valid and the value if desired
int64_t parse_headerField(char *header, int *value) {
    int index = 0, length = 0;
    // Calculates the length of the header excluding \r\n
    while (header[length + 1] != '\n' || header[length] != '\r') {
        length += 1;
    }
    header[length] = '\0'; // Null terminate the string to remove the \r\n

    while (header[index] != ':' && index < length) {
        index += 1;
    }
    // Null terminate the key
    header[index] = '\0';
    if (index + 2 >= length) {
        // A valid header needs at least 2 more characters for a space and key value
        return INVALID;
    }
    // remove any spaces after the colon
    index += 1; // Colon character
    while (header[index] == ' ' && index < length) {
        index += 1;
    }
    if (index >= length) {
        // No key value
        return INVALID;
    }
    int64_t header_type = -1;
    bool content = !strcmp(header, "Content-Length"), id = !strcmp(header, "Request-Id");
    if (content || id) {
        // Either a request-id header or content-length header
        while (!isdigit(header[index]) && index < length) {
            // Consider a header field invalid if the key ends with a non-digit value or contains a null character
            if (header[index] == '\0' || index + 1 == length) {
                return INVALID;
            }
            index += 1;
        }
        char *number = (char *) calloc(length - index, sizeof(char));
        int ind = 0;
        while (index < length) {
            if (!isdigit(header[index])) {
                break;
            }
            number[ind] = header[index];
            ind += 1;
            index += 1;
        }
        if (ind < 1) {
            // No digit in value section
            return INVALID;
        }
        uint64_t tens = 1;
        for (int digit = ind - 1; digit > -1; --digit) {
            if (!isgraph(number[digit])) {
                header_type = INVALID;
                break;
            } else if (number[digit] - 48 < 0 || !isdigit(number[digit])) {
                // Non digit value inbetween the key value
                header_type = -1;
                break;
            }
            *value += (number[digit] - 48) * tens;
            tens *= 10;
        }
        header_type = id ? ID : LENGTH;
        free(number);
    } else {
        for (; index < length; ++index) {
            // Any other header fields must not contain a null byte or a non-ascii character
            if (!isascii(header[index]) || header[index] == '\0') {
                header_type = INVALID;
            }
        }
    }
    return header_type;
}

// Parses the request line for the method, uri, version
// returns whether the request line was valid or not
int parse_requestLine(char **uri, char *request) {
    int length = 0;
    char type[REQUEST_MAX] = { 0 };
    int limit = REQUEST_MAX; // Ensures the request line is at most 2048 characters
    // Calculates the length of the request line excluding \r\n
    while (request[length + 1] != '\n' || request[length] != '\r') {
        length += 1;
    }
    // Method
    // Only accepting the methods HEAD, GET, and PUT
    for (int i = 0; i < REQUEST_MAX; ++i) {
        length -= 1;
        limit -= 1;
        if (limit < 0 || request[0] == ' ') {
            // Found space delimiter between the method and URI
            break;
        }
        type[i] = toupper(request[0]);
        request += 1;
    }
    request += 1;

    // URI
    char path[REQUEST_MAX] = { 0 };
    for (int i = 0; i < REQUEST_MAX; ++i) {
        limit -= 1;
        length -= 1;
        if (limit < 0 || length < 0 || request[0] == ' ') {
            break;
        }
        path[i] = request[0];
        request += 1;
    }
    // Save a copy of the URI for the caller
    *uri = strdup(path);
    request += 1;

    // Version
    char version[10] = { 0 };
    for (int i = 0; i < 9; ++i) {
        if (limit < 0 || length <= 0 || request[0] == ' ') {
            break;
        }
        version[i] = toupper(request[0]);
        length -= 1;
        request += 1;
        limit -= 1;
    }

    if (limit < 0 || strcmp(version, "HTTP/1.1")) {
        return INVALID;
    }

    int method = NOT_IMPLEMENTED;
    // Determine which method this request wants
    if (!strcmp(type, "PUT")) {
        method = PUT;
    } else if (!strcmp(type, "HEAD")) {
        method = HEAD;
    } else if (!strcmp(type, "GET")) {
        method = GET;
    } else if (!strcmp(type, "OPTIONS")) {
        method = OPTIONS;
    } else if (!strcmp(type, "APPEND")) {
        method = APPEND;
    }
    return method;
}

// Parses a uri to create directories and to ensure the path is valid
// returns whether the path was valid or not
bool parse_uri(char *path, int request) {
    struct stat sb;
    char path_name[REQUEST_MAX] = { 0 };
    // Ensures the path is valid
    for (unsigned long chr = 1; chr < strlen(path); ++chr) {
        if (path[chr] == '/') {
            stat(path_name, &sb);
            if (S_ISREG(sb.st_mode)) {
                // Is a file
                return false;
            }
        }
        path_name[chr - 1] = path[chr];
    }
    if (request != PUT) {
        // Dont need to create files/directories for non-put requests
        return true;
    }
    for (unsigned long chr = 0; chr < strlen(path_name); ++chr) {
        // Creates the directories in the file path if they don't exist
        if (path_name[chr] == '/') {
            // Makes the directory specified in the file path
            char temp = path_name[chr];
            path_name[chr] = '\0';
            mkdir(path_name, 0755);
            path_name[chr] = temp;
        }
    }
    return true;
}
