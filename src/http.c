#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include "http.h"
#include "file_utils.h"

void prepare_http_redirect(client_t *client, const char *location_url){
    char header[1024];
    int header_len = snprintf(header, sizeof(header),
            "HTTP/1.1 302 Found\r\n"
            "Location: %s\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n"
            "\r\n",
            location_url);
    client->header = malloc(header_len+1);
    memcpy(client->header, header, header_len);
    client->header_len = header_len;
    client->header_offset = 0;

    client->file = NULL;
    client->file_size = 0;
    client->file_offset = 0;

    client->state = SENDING_HEADER;
}

void prepare_http_response(client_t *client, unsigned int status_code, char *status_msg, char *content_t, FILE *file, off_t file_size){

    // Prepare file
    client->file = file;
    client->file_size = file_size;
    client->file_offset = 0;
    client->file_buffer_len = 0;
    client->file_buffer_offset = 0;

    // Prepare header
    char header[1024];
    int header_len = snprintf(header, sizeof(header), 
            "HTTP/1.1 %u %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %" PRIuMAX "\r\n"
            "Connection: close\r\n"
            "\r\n"
            , status_code, status_msg, content_t, (uintmax_t)file_size);
    client->header = malloc(header_len+1);
    memcpy(client->header, header, header_len);
    client->header_len = header_len;
    client->header_offset = 0;

    client->state = SENDING_HEADER;
}

// RETURN VALUES: 0 (not finished), 1 (finished), -1 (error)
int send_header_chunk(client_t *client){
    // Check if the header is fully sent
    if(client->header_offset >= client->header_len){
        client->state = SENDING_FILE;
        return 1;
    }

    // Sending the header
    ssize_t n_write = write(client->fd,
            client->header + client->header_offset,
            client->header_len - client->header_offset);
    // Not finish sending header
    if(n_write < 0){
        if(errno != EAGAIN || errno != EWOULDBLOCK){
            perror("Cannot write to the socket");
            return -1;
        }
    }
    client->header_offset += n_write;
    return 0;
}

// RETURN VALUES: 0 (not finished), 1 (finished), -1 (error)
int send_file_chunk(client_t *client){
    
    /* Check if the buffer has fully sent */
    if(client->file_buffer_offset >= client->file_buffer_len){
        /* Check if the file has fully sent */
        if(client->file_offset >= client->file_size){
            client->state = CONN_DONE;
            return 1;
        }

        size_t to_read = sizeof(client->file_buffer);
        if(client->file_offset + to_read > client->file_size){
            to_read = client->file_size - client->file_offset;
        }

        client->file_buffer_len = fread(client->file_buffer, 1, to_read, client->file);
        client->file_buffer_offset = 0;
        if(client->file_buffer_len == 0){
            if(feof(client->file)){
                client->state = CONN_DONE;
                return 1;
            }else{
                perror("Cannot read the file");
                return -1;
            }
        }
    }

    ssize_t n_write = write(client->fd, client->file_buffer, client->file_buffer_len);
    if(n_write < 0){
        if(errno != EAGAIN || errno != EWOULDBLOCK){
            perror("Cannot write to the socket");
            return -1;
        }
    }

    client->file_buffer_offset += n_write;
    client->file_offset += n_write;
    printf("File progress %ld/%ld bytes sent\n", client->file_offset, client->file_size);
    return 0;
}

void handle_http_request(client_t *client, const char *request){
    /*
    GET /index.html HTTP/1.1
    Host: localhost:8080
    User-Agent: Mozilla/5.0 (Chrome)
    Accept: text/html
    Connection: close
    */
    char method[16], path[256], file_state[16], version[16], full_path[sizeof(path)+strlen(BASE_PATH)+1];
    char *req = strndup(request, strlen(request)+1); // we do not want to modify the original `request` on `strtok`
    char *first_line = strtok(req, "\r\n"); // GET /index.html HTTP/1.1
    if(sscanf(first_line, "%15s %255s %15s", method, path, version) != 3){
        perror("sscanf()");
    }
    if(strcmp(path, "/") == 0){
        strcpy(path, "/index.html");
    }
    
    //char *base = BASE_PATH;
    strcpy(full_path, BASE_PATH);
    strcat(full_path, path);

    printf("%s\n", full_path);

    unsigned int http_status;
    char status_msg[64], content_t[64];
    FILE *file = fopen(full_path, "rb");
    off_t f_size;
    if(strcmp(method, "GET") == 0){
        if(strcmp(path, "/oldpage.html") == 0){
            prepare_http_redirect(client, "/index.html");
            free(req);
            return;
        }else if(file != NULL){ // (strcmp(path, "/") == 0
            // 200 OK

            // Find file size
            f_size = find_file_size(file);
            if(f_size < 0){
                free(req);
                return;
            }
            printf("File size : %ld\n", f_size);

            // Setting Values
            strcpy(file_state, "valid");
            http_status = 200;
            strcpy(status_msg, "OK");
            strcpy(content_t, get_content_type(path));

        }else{
            // 404 - Page Not Found

            // Find file size
            file = fopen(FILE_404, "rb");
            f_size = find_file_size(file);
            if(f_size < 0){
                free(req);
                return;
            }

            // Set values
            strcpy(file_state, "not valid");
            http_status = 404;
            strcpy(status_msg, "Not Found");
            strcpy(content_t, get_content_type(FILE_404));

        }
    }
    /*else if(strcmp(method, "POST") == 0){ 
    }*/
    else{
        //405 - Method Not Allowed
        
        // Find file size
        file = fopen(FILE_405, "rb");
        f_size = find_file_size(file);
        if(f_size < 0){
            free(req);
            return;
        }

        // Set values
        http_status = 405;
        strcpy(status_msg, "Method Not Allowed");
        strcpy(content_t, get_content_type(FILE_405));
    }
    printf("Method : %s\nPath: %s [%s]\nVersion: %s\n", method, path, file_state, version);
    prepare_http_response(client, http_status, status_msg, content_t, file, f_size);
    free(req);
}

