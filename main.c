#include <stdio.h>
#include <stdlib.h>
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

typedef struct {
    unsigned int fd;
    char ip[INET_ADDRSTRLEN];
    unsigned short port;
}client_t;

void display_clients(client_t *clients, unsigned int n_clients){
    printf("--------------Clients---------------\n");
    for(int i = 0; i < n_clients; i++){
        printf("FD : %d => %s:%d\n", clients[i].fd, clients[i].ip, clients[i].port);
    }
    printf("------------------------------------\n");
}

void disconnect(unsigned int c_fd, client_t **clients, unsigned int *n_clients, struct pollfd **pfds, unsigned int *n_pfds){
    // Search the client socket
    for(int i = 0; i < *n_clients; i++){
        // when the client has found, do as follows and break the loop (otherwise iterate the next loop)
        if(c_fd == (*clients)[i].fd){

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

void send_http_response(int cli_sock, unsigned int status_code, char *status_msg, char *content_t, FILE *file, off_t file_size){
    char buffer[4096];
    //size_t body_len = body ? strlen(body) : 0;

    // Send header
    snprintf(buffer, sizeof(buffer), 
            "HTTP/1.1 %u %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %" PRIuMAX "\r\n"
            "Connection: close\r\n"
            "\r\n"
            , status_code, status_msg, content_t, file_size);
    if(write(cli_sock, buffer, strlen(buffer)) == -1){  // not sending null terminator which is not part of the http protocol
        perror("Error sending header");
        return;
    }
    printf("Header sent\n");

    // Send body by streaming
    size_t bytes_read;
    while((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0){
        printf("bytes_read : %d\n", bytes_read);

        // this loop below enables us to send `bytes_read` separately in case of some issues
        size_t total_written = 0;
        while(total_written < bytes_read){
            ssize_t bytes_sent = write(cli_sock, buffer+total_written, bytes_read-total_written);
            if(bytes_sent < 0){
                if(errno == EINTR) continue; // retry 
                perror("Error sending file");
                break;
            }
            printf("bytes_sent : %d\n", bytes_sent);
            total_written += bytes_sent;
        } 
        // this is better than sending whole `bytes_read` at once as in `write(cli_sock, buffer, bytes_read);`
    }
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

void handle_http_request(unsigned int cli_sock, const char *request){
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
    sscanf(first_line, "%16s %256s %16s", method, path, version);
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

            //send_http_response(cli_sock, 404, "Not Found", "text/html", file, f_size);
        }
    }
    /*else if(strcmp(method, "POST") == 0){ 
        send_http_response(cli_sock, 200, "OK", "text/html", body);
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
    send_http_response(cli_sock, http_status, status_msg, content_t, file, f_size);
    fclose(file);
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
    unsigned int serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1){
        perror("Failed to create a socket for server");
        exit(EXIT_FAILURE);
    }

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

        int n_ready = poll(pfds, n_pfds, -1);
        if(n_ready < 0){
            perror("poll() error");
            exit(EXIT_FAILURE);
            free(clients);
            free(pfds);
        }else if(n_ready > 0){
            for(int i = 0; i < n_pfds; i++){
                if(pfds[i].revents & POLLIN){      // there is any data to read
                    if(pfds[i].fd == serv_sock){   // new connection

                        struct sockaddr_in cli_addr;
                        socklen_t cli_addr_len = sizeof(cli_addr);

                        unsigned int cli_sock = accept(serv_sock, (struct sockaddr *) &cli_addr, &cli_addr_len);
                        if(cli_sock == -1){
                            perror("Accept failed");
                            continue;
                        }

                        // Getting and displaying the client's address information
                        char cli_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &cli_addr.sin_addr, cli_ip, sizeof(cli_ip));
                        unsigned short cli_port = ntohs(cli_addr.sin_port);

                        printf("========================\n");
                        printf("New connection\n");
                        printf("FD = %d, %s:%d\n", cli_sock, cli_ip, cli_port);

                        // Resize and update the `clients`
                        void *tmp = realloc(clients, sizeof(client_t) * (++n_clients));
                        if(!tmp){
                            perror("client realloc() error");
                            exit(EXIT_FAILURE);
                            free(clients);
                            free(pfds);
                        }
                        clients = tmp;

                        clients[n_clients-1].fd = cli_sock;
                        strcpy(clients[n_clients-1].ip, cli_ip);
                        clients[n_clients-1].port = cli_port;

                        // Resize and update the `pfds`
                        tmp = realloc(pfds, sizeof(struct pollfd) * (++n_pfds));
                        if(!tmp){
                            perror("pfds realloc() error");
                            exit(EXIT_FAILURE);
                            free(clients);
                            free(pfds);
                        }
                        pfds = tmp;
                        pfds[n_pfds-1].fd = cli_sock;
                        pfds[n_pfds-1].events = POLLIN;

                        // Dispalying all clients
                        display_clients(clients, n_clients);

                    }else{
                        unsigned int cli_sock = pfds[i].fd;
                        // send, receive and disconnect
                        while(1){
                            char buffer[BUFFER_SIZE];
                            unsigned n_read = read(cli_sock, buffer, sizeof(buffer));

                            if(n_read == 0){
                                // Disconnects
                                printf("%d\n", cli_sock);
                                disconnect(cli_sock, &clients, &n_clients, &pfds, &n_pfds);
                                printf("[%d] Client disconnected\n", cli_sock);
                                printf("n_pfds : %d\n", n_pfds);
                                printf("n_clients : %d\n", n_clients);
                                // adjust the loop logic
                                i--;
                                break;
                            }else{
                                // Handling Messages
                                buffer[n_read] = 0; // null termination
                                handle_http_request(cli_sock, buffer);

                                // Disconnecting
                                disconnect(cli_sock, &clients, &n_clients, &pfds, &n_pfds);
                                i--;
                                break;
                            }

                        }
                    }
                }
            } 
        }
    }

    free(clients);
    free(pfds);
    return 0;
}
