#pragma once

#include "common.h"
#include "server.h"

typedef struct http_request {
    char resource_id[1 << 10];
    char host[1 << 10];
    char connection[1 << 10];
} http_request_t;

typedef struct http_response {
    int status;
    char resource_path[1 << 10];
} http_response_t;

enum {
    TOKEN_GET,
    TOKEN_HOST,
    TOKEN_CONNECTION,
    TOKEN_NULL
};

enum {
    HTTP_NOT_IMPLEMENTED = 501,
    HTTP_NOT_FOUND = 404,
    HTTP_FORBIDDEN = 403,
    HTTP_REDIRECT = 301,
    HTTP_OK = 200
};

#define HTML_NOT_IMPLEMENTED "<h1> 501 Not Implemented </h1>"
#define HTML_NOT_FOUND       "<h1> 404 Not Found       </h1>"
#define HTML_FORBIDDEN       "<h1> 403 Forbidden       </h1>"

void handle_client(int connect_fd, server_t *server);

// read one http message from socket stream
char *read_message(int connect_fd);

// fill the request object
void parse(char *message, http_request_t *request);

// build filesystem resource path and decide return status
void analyse(server_t *server, http_request_t *request, http_response_t *response);

// craft response for each possibility
char *craft_response(http_response_t *response, int *size);

// reliable sender
void send_response(int connect_fd, char *response, int to_send);

