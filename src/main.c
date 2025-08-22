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

#include "server_config.h"
#include "client.h"
#include "http.h"
#include "network.h"


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
