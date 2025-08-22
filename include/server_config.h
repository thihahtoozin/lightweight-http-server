#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <stdio.h>      // FILE 
#include <netinet/in.h> // INET_ADDRSTRLEN
#include <sys/types.h>  // off_t, size_t

#define BACKLOGS 1
#define BUFFER_SIZE 256
#define BASE_PATH "config/www/html"
#define BASE_CONFIG "config"
#define FILE_404 "config/404.html"
#define FILE_405 "config/405.html"

typedef enum {
    READING_REQ,
    SENDING_HEADER,
    SENDING_FILE,
    CONN_DONE
}client_state_t;

typedef struct {
    int fd;
    char ip[INET_ADDRSTRLEN];
    unsigned short port;

    // for tracking state
    client_state_t state;
    FILE *file;
    off_t file_size;
    size_t file_offset;

    char file_buffer[4096];
    size_t file_buffer_len;
    size_t file_buffer_offset;

    char *header;
    size_t header_len;
    size_t header_offset;
}client_t;

#endif
