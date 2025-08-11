#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#define BACKLOGS 1
#define BUFFER_SIZE 256

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
            // Close the client socket
            close((*clients)[i].fd);

            // Remove the client struct from the `clients`
            for(int k = i; k < *n_clients; k++){
                (*clients)[k] = (*clients)[k+1];
            }
            void *tmp = realloc(*clients, sizeof(client_t)*(--*n_clients));
            //if(!tmp){
            //    perror("[disconnects] clients realloc() error");
            //    exit(EXIT_FAILURE);
            //}
            *clients = tmp;

            // Remove the client fd from the `pfds`
            for(int j = 0; j < *n_pfds; j++){
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
            }
            *pfds = tmp;
            break;
        }
    }
}

int main(int argc, char **argv){

    // Resolving arguments
    if(argc < 3){
        fprintf(stderr, "Usage %s <ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *serv_ip = argv[1];
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
        }else if(n_ready > 0){
            for(int i = 0; i < n_pfds; i++){
                if(pfds[i].revents & POLLIN){      // there is any data to read
                    if(pfds[i].fd == serv_sock){     // new connection

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

                        printf("New connection\n");
                        printf("FD = %d, %s:%d\n", cli_sock, cli_ip, cli_port);

                        // Resize and update the `clients`
                        void *tmp = realloc(clients, sizeof(client_t) * (++n_clients));
                        if(!tmp){
                            perror("client realloc() error");
                            exit(EXIT_FAILURE);
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
                                // Messages
                                buffer[n_read] = 0; // null termination
                                char msg[n_read];
                                strcpy(msg, buffer);

                                printf("[%d] %s", cli_sock, msg);

                                write(cli_sock, msg, sizeof(msg));

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

    return 0;
}
