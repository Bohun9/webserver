#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/select.h>
#include <assert.h>
#include <stdlib.h>

#include "http_handler.h"

char *read_message(int connect_fd) {
    static char buffer[1 << 20];

    for (int cur = 0;; cur++) {
        buffer[cur] = buffer[cur + 1] = '\0';
        if (aver(read(connect_fd, buffer + cur, 1), "read") == 0) {
            break;
        }
        if (cur >= 3 && strcmp(buffer + cur - 3, "\r\n\r\n") == 0) {
            break;
        }
    }

    return buffer;
}

// for simplicity assume that SPs, CRs and LFs can not appear in tokens
void parse(char *message, http_request_t *request) {
    char *delim = " \r\n";
    char *token = strtok(message, delim);
    int last_token = TOKEN_NULL;

    while (token) {
        switch (last_token) {
            case TOKEN_GET:        strcpy(request->resource_id, token); break;
            case TOKEN_HOST:       strcpy(request->host,        token); break;
            case TOKEN_CONNECTION: strcpy(request->connection,  token); break;
        }

        last_token = TOKEN_NULL;
        if (!strcmp(token, "GET"))         last_token = TOKEN_GET;
        if (!strcmp(token, "Host:"))       last_token = TOKEN_HOST;
        if (!strcmp(token, "Connection:")) last_token = TOKEN_CONNECTION;
        token = strtok(NULL, delim);
    }
}

void trim_slash(char *s) {
    if (s[strlen(s) - 1] == '/') {
        s[strlen(s) - 1] = '\0';
    }
}

void delete_port(char *s) {
    char *colon = strchr(s, ':');
    if (colon == NULL) return;
    *colon = '\0';
}

void analyse(server_t *server, http_request_t *request, http_response_t *response) {
    if (!strlen(request->resource_id) || !strlen(request->host)) {
        response->status = HTTP_NOT_IMPLEMENTED;
        return;
    }

    delete_port(request->host);

    strcat(response->resource_path, server->directory);
    trim_slash(response->resource_path);
    strcat(response->resource_path, "/");
    strcat(response->resource_path, request->host);
    strcat(response->resource_path, request->resource_id);
    trim_slash(response->resource_path);

    debug_msg("[RESOURCE QUERY] pid: %d path:%s\n", getpid(), response->resource_path);

    if (!exists(response->resource_path)) {
        response->status = HTTP_NOT_FOUND;
        return;
    }
    if (strstr(request->resource_id, "..")) {
        response->status = HTTP_FORBIDDEN;
        return;
    }
    if (is_directory(response->resource_path)) {
        response->status = HTTP_REDIRECT;
        strcpy(response->resource_path, request->resource_id);
        trim_slash(response->resource_path);
        strcat(response->resource_path, "/index.html");
        return;
    }
    response->status = HTTP_OK;
}

int timeout_occured(int connect_fd, server_t *server) {
    struct timeval wait = {
        .tv_sec = server->timeout.tv_sec,
        .tv_usec = server->timeout.tv_usec
    };

    fd_set set;
    FD_ZERO(&set);
    FD_SET(connect_fd, &set);

    return select(connect_fd + 1, &set, NULL, NULL, &wait) == 0;
}

char *status_string(int status) {
    switch (status) {
        case HTTP_NOT_IMPLEMENTED: return "Not Implemented";
        case HTTP_NOT_FOUND:       return "Not Found";
        case HTTP_FORBIDDEN:       return "Forbidden";
        case HTTP_REDIRECT:        return "Moved Permanently";
        case HTTP_OK:              return "OK";
    }

    fprintf(stderr, "Internal server error: Unknown response status: %d\n", status);
    exit(EXIT_FAILURE);
}

char *file_extension(char *path) {
    char *ext = strrchr(path, '.');
    if (ext == NULL) return "application/octet-stream";
    if (!strcmp(ext, ".txt"))  return "text/plain;charset=utf-8";
    if (!strcmp(ext, ".html")) return "text/html;charset=utf-8";
    if (!strcmp(ext, ".css"))  return "text/css;charset=utf-8";
    if (!strcmp(ext, ".jpg"))  return "image/jpeg";
    if (!strcmp(ext, ".jpeg")) return "image/jpeg";
    if (!strcmp(ext, ".png"))  return "image/png";
    if (!strcmp(ext, ".pdf"))  return "application/pdf";
    return "application/octet-stream";
}

char *craft_response(http_response_t *response, int *size) {
    static char buffer[1 << 20];
    sprintf(buffer, "HTTP/1.1 %d %s\r\n", response->status, status_string(response->status));

    if (response->status == HTTP_OK) {
        sprintf(buffer + strlen(buffer), "Content-Type: %s\r\n", file_extension(response->resource_path));
        sprintf(buffer + strlen(buffer), "Content-Length: %lu\r\n\r\n", file_size(response->resource_path));

        *size = strlen(buffer) + file_size(response->resource_path);

        FILE *file = fopen(response->resource_path, "r");
        assert(file != NULL);
        aver(fread(buffer + strlen(buffer), sizeof(char), file_size(response->resource_path), file), "fread");

        return buffer;
    }

    if (response->status == HTTP_REDIRECT) {
        sprintf(buffer + strlen(buffer), "Location: %s\r\n\r\n", response->resource_path);
        *size = strlen(buffer);
        return buffer;
    }

    strcat(buffer, "Content-Type: text/html;charset=utf-8");
    switch (response->status) {
        case HTTP_NOT_IMPLEMENTED: sprintf(buffer + strlen(buffer), "Content-Length: %lu\r\n\r\n%s", strlen(HTML_NOT_IMPLEMENTED), HTML_NOT_IMPLEMENTED); break;
        case HTTP_NOT_FOUND:       sprintf(buffer + strlen(buffer), "Content-Length: %lu\r\n\r\n%s", strlen(HTML_NOT_FOUND)      , HTML_NOT_FOUND);       break;
        case HTTP_FORBIDDEN:       sprintf(buffer + strlen(buffer), "Content-Length: %lu\r\n\r\n%s", strlen(HTML_FORBIDDEN)      , HTML_FORBIDDEN);       break;
    }
    *size = strlen(buffer);
    return buffer;
}

void send_response(int connect_fd, char *raw_response, int to_send) {
    while (to_send > 0) {
        ssize_t send = aver(write(connect_fd, raw_response, to_send), "write");
        raw_response += send;
        to_send -= send;
    }
}

int connection_close(http_request_t *request) {
    return strcmp(request->connection, "close") == 0;
}

void handle_client(int connect_fd, server_t *server) {
    debug_msg("[NEW CLIENT] pid:%d\n", getpid());

    for (;;) {
        char *message = read_message(connect_fd);
        if (!strlen(message)) { // EOF
            break;
        }

        http_request_t request;
        bzero(&request, sizeof(request));
        parse(message, &request);

        http_response_t response;
        bzero(&response, sizeof(response));
        analyse(server, &request, &response);

        int size;
        char *raw_response = craft_response(&response, &size);
        send_response(connect_fd, raw_response, size);

        if (connection_close(&request) || timeout_occured(connect_fd, server)) {
            break;
        }
    }

    debug_msg("[FINISH CLIENT] pid:%d\n", getpid());
}

