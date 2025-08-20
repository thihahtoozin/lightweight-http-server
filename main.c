#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h> // For PRIuMAX

#include <sys/socket.h>
#include <arpa/inet.h>

#define BACKLOGS 1
#define BUFFER_SIZE 256
#define BASE_PATH "www/html"
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
    size_t file_size;
    size_t file_offset;

    char file_buffer[4096];
    size_t file_buffer_len;
    size_t file_buffer_offset;

    char *header;
    size_t header_len;
    size_t header_offset;
}client_t;

void display_clients(client_t *clients, unsigned int n_clients){
    printf("--------------Clients---------------\n");
    for(int i = 0; i < n_clients; i++){
        printf("FD : %d => %s:%d\n", clients[i].fd, clients[i].ip, clients[i].port);
    }
    printf("------------------------------------\n");
}

void cleanup_client(client_t *client){
    if(client->file){
        fclose(client->file);
        client->file = NULL;
    }
    if(client->header){
        free(client->header);
        client->header = NULL;
    }
}

int set_nonblocking(int fd){
    // fd must be opened
    int flags = fcntl(fd, F_GETFL);
    flags |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, flags) < 0){
        perror("F_SETFL");
        return 1;
    }
    return 0;
}

void disconnect(int c_fd, client_t **clients, unsigned int *n_clients, struct pollfd **pfds, unsigned int *n_pfds){
    // Search the client socket
    for(int i = 0; i < *n_clients; i++){
        // when the client has found, do as follows and break the loop (otherwise iterate the next loop)
        if(c_fd == (*clients)[i].fd){

            // Clean up client resources
            cleanup_client(&(*clients)[i]);

            // Making sure all the buffer have flushed(sent).
            shutdown(c_fd, SHUT_WR); // This sends FIN pkt to the client
            
            // Keep reading client's remaining data
            char buffer[4096];
            ssize_t n_read;
            while((n_read = read(c_fd, buffer, sizeof(buffer))) > 0){
                // doing nth with client's data, we're just getting
            }

            // Close the client socket
            close(c_fd); //close((*clients)[i].fd);

            // Remove the client struct from the `clients`
            for(int k = i; k < *n_clients-1; k++){  // *n_clients-1 because of (*clients)[k+1]
                (*clients)[k] = (*clients)[k+1];
            }
            void *tmp = realloc(*clients, sizeof(client_t)*(--*n_clients));
            *clients = tmp;

            // Remove the client fd from the `pfds`
            for(int j = 0; j < *n_pfds-1; j++){     // *n_pfds-1 because of (*pfds)[k+1]
                if((*pfds)[j].fd == c_fd){
                    for(int k = j; k < *n_pfds; k++){
                        (*pfds)[k] = (*pfds)[k+1];
                    }
                break;
                }
            }
            tmp = realloc(*pfds, sizeof(struct pollfd) * (--*n_pfds));
            if(!tmp){
                perror("[disconnects] pfds realloc() error");
                exit(EXIT_FAILURE);
                free(*clients);
                free(*pfds);
            }
            *pfds = tmp;
            break;
        }
    }
}

char *get_content_type(const char *filename){
    //char content_type[32];
    const char *ext = strrchr(filename, '.')+1;
    if(!ext || ext == filename) return "";

    if((strcmp(ext, "htm") == 0) || (strcmp(ext, "html")==0)){
        return "text/html";
    }else if(strcmp(ext, "css") ==0){
        return "text/css";
    }else if(strcmp(ext, "js") == 0){
        return "application/javascript";
    } 
    return "";
    //return content_type;
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

off_t find_file_size(FILE *fp){
    off_t f_size;
    if(fseeko(fp, 0, SEEK_END) == 0){
        f_size = ftello(fp);
        fseeko(fp, 0, SEEK_SET);
        if(f_size == -1){
            perror("Cannot get the size of the file by ftello()");
            return -1;
            }
    }else{
        perror("Cannot find SEEK_END by fseeko()");
        return -1;
    }
    return f_size;
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
        if(file != NULL){ // (strcmp(path, "/") == 0
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

int main(int argc, char **argv){

    // Resolving arguments
    if(argc < 3){
        fprintf(stderr, "Usage %s <ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    const char *serv_ip = argv[1];
    unsigned short serv_port = atoi(argv[2]);
    
    // Establishing server socket
    int serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1){
        perror("Failed to create a socket for server");
        exit(EXIT_FAILURE);
    }

    // Setting Non-blocking flag
    set_nonblocking(serv_sock);

    // Setting socket option
    int opt = 1;
    if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }
    
    // Server address structure
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serv_port);
    inet_pton(AF_INET, serv_ip, &serv_addr.sin_addr);

    // Binding
    if(bind(serv_sock, (const struct sockaddr *) &serv_addr, (socklen_t) sizeof(serv_addr)) == -1){
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }

    // Listening
    if(listen(serv_sock, BACKLOGS) == -1){
        perror("listen() failed");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on %s:%d\n", serv_ip, serv_port);

    unsigned int n_clients = 0; // this is to track the number of clients connected
    client_t *clients = NULL;   // this is to hold the address of client structures stored in memory

    unsigned int n_pfds = 1;    // this is to track the number of poll fds (1 because we have `serv_sock` to add to poll)
    struct pollfd *pfds = NULL; // this is to hold the address of poll structures stored in memory
    
    pfds = realloc(pfds, sizeof(struct pollfd)); // allocate the memory for 1 pollfd structure
    memset(pfds, 0, sizeof(struct pollfd));      // clearing the memory before adding any values

    pfds->fd = serv_sock;       // adding server socket fd
    pfds->events = POLLIN;      // adding event type 0x0001

    while(1){
        // Check each client status and set POLLIN/POLLOUT accordingly
        for(int i = 1; i < n_pfds; i++){
            for(int j = 0; j < n_clients; j++){
                if(clients[j].fd == pfds[i].fd){
                    switch(clients[j].state){
                        case READING_REQ:
                            pfds[i].events = POLLIN;
                            break;
                        case SENDING_HEADER:
                        case SENDING_FILE:
                            pfds[i].events = POLLOUT;
                            break;
                        case CONN_DONE:
                            pfds[i].events = 0;
                            break;
                    }
                }
            }
        }

        int n_ready = poll(pfds, n_pfds, -1);
        if(n_ready < 0){
            perror("poll() error");
            exit(EXIT_FAILURE);
            free(clients);
            free(pfds);
        }else if(n_ready > 0){
            for(int i = 0; i < n_pfds; i++){
                if(pfds[i].fd == serv_sock && (pfds[i].revents & POLLIN)){      // there is any data to read

                    // New Connection
                    struct sockaddr_in cli_addr;
                    socklen_t cli_addr_len = sizeof(cli_addr);

                    // Accepting the connection
                    int cli_sock = accept(serv_sock, (struct sockaddr *) &cli_addr, &cli_addr_len);
                    if(cli_sock == -1){
                        perror("Accept failed");
                        continue;
                    }

                    // Make client socket non-blocking
                    set_nonblocking(cli_sock);

                    // Getting and displaying the client's address information
                    char cli_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &cli_addr.sin_addr, cli_ip, sizeof(cli_ip));
                    unsigned short cli_port = ntohs(cli_addr.sin_port);

                    printf("========================\n");
                    printf("New connection\n");
                    printf("FD = %d, %s:%d\n", cli_sock, cli_ip, cli_port);

                    // Resize and update the `clients`
                    void *tmp = realloc(clients, sizeof(client_t) * (n_clients+1));
                    if(!tmp){
                        perror("client realloc() error");
                        free(clients);
                        free(pfds);
                        exit(EXIT_FAILURE);
                    }
                    clients = tmp;

                    // Setting up new clients
                    clients[n_clients].fd = cli_sock;
                    strcpy(clients[n_clients].ip, cli_ip);
                    clients[n_clients].port = cli_port;

                    clients[n_clients].state = READING_REQ;

                    clients[n_clients].file = NULL;
                    clients[n_clients].file_size = 0;
                    clients[n_clients].file_offset = 0;

                    clients[n_clients].file_buffer_len = 0;
                    clients[n_clients].file_buffer_offset = 0;

                    clients[n_clients].header = NULL;
                    clients[n_clients].header_len = 0;
                    clients[n_clients].header_offset = 0;

                    n_clients++;

                    // Resize and update the `pfds`
                    tmp = realloc(pfds, sizeof(struct pollfd) * (n_pfds+1));
                    if(!tmp){
                        perror("pfds realloc() error");
                        free(clients);
                        free(pfds);
                        exit(EXIT_FAILURE);
                    }
                    pfds = tmp;
                    pfds[n_pfds].fd = cli_sock;
                    pfds[n_pfds].events = POLLIN;
                    pfds[n_pfds].revents = 0;
                    n_pfds++;

                    // Dispalying all clients
                    display_clients(clients, n_clients);

                //--------    
                }else if(pfds[i].revents & (POLLIN | POLLOUT)){
                    /* Reading and Writing with clients */

                    // Find client
                    client_t *client;
                    for(int j = 0; j < n_clients; j++){
                        if(clients[j].fd == pfds[i].fd){
                            client = &clients[j];
                        }
                    }

                    if(client->state == READING_REQ && pfds[i].revents == POLLIN){

                        /* Receive data or Disconnect */
                        char buffer[BUFFER_SIZE];
                        ssize_t n_read = read(client->fd, buffer, sizeof(buffer));

                        if(n_read == 0){
                            /* Disconnects */
                            printf("%d\n", client->fd);
                            disconnect(client->fd, &clients, &n_clients, &pfds, &n_pfds);
                            //printf("[%d] Client disconnected\n", client->fd);
                            printf("Client disconnected\n");
                            printf("n_pfds : %d\n", n_pfds);
                            printf("n_clients : %d\n", n_clients);
                            i--; // adjust the loop counter
                        }else if(n_read < 0){
                            /* No data to read for now. Can move on to next poll */
                            if(errno != EAGAIN || errno != EWOULDBLOCK){
                                // Cannot read from the socket
                                perror("Cannot read from the socket");
                                disconnect(client->fd, &clients, &n_clients, &pfds, &n_pfds);
                                i--;
                            }
                        }else{
                            /* Handling Messages */
                            buffer[n_read] = 0; // null termination
                            handle_http_request(client, buffer);
                        }
                    }else if(pfds[i].revents & POLLOUT){
                        int result = 0;
                        if(client->state == SENDING_HEADER){
                            result = send_header_chunk(client);
                        }else if(client->state == SENDING_FILE){
                            result = send_file_chunk(client);
                        }
                        if(result == 1){
                            if(client->state == CONN_DONE){
                                disconnect(client->fd, &clients, &n_clients, &pfds, &n_pfds);
                                i--;
                            }
                        }else if(result == -1){
                            fprintf(stderr, "Error sending to the client %d\n", client->fd);
                            disconnect(client->fd, &clients, &n_clients, &pfds, &n_pfds);
                            i--;
                        }
                    }
                } 
            }
        }
    }

    for(int i = 0; i < n_clients; i++){
        cleanup_client(&clients[i]);
    }
    free(clients);
    free(pfds);
    close(serv_sock);
    return 0;
}
