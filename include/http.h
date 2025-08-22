#ifndef HTTP_H
#define HTTP_H

#include <stdio.h>
#include <sys/types.h>
#include "server_config.h"

void prepare_http_redirect(client_t *client, const char *location_url);
void prepare_http_response(client_t *client, unsigned int status_code, char *status_msg, char *content_t, FILE *file, off_t file_size);
int send_header_chunk(client_t *client);
int send_file_chunk(client_t *client);
void handle_http_request(client_t *client, const char *request);

#endif
