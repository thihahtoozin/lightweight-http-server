#ifndef CLIENT_H
#define CLIENT_H

#include <poll.h>
#include "server_config.h"

void display_clients(client_t *clients, unsigned int n_clients);
void cleanup_client(client_t *client);
void disconnect(int c_fd, client_t **clients, unsigned int *n_clients, struct pollfd **pfds, unsigned int *n_pfds);

#endif
