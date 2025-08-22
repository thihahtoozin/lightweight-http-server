#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include "client.h"

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

