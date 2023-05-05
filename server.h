#pragma once

#include <sys/time.h>

#define QUEUE_SIZE 16

typedef struct server {
    char *directory;
    int listen_fd;
    struct timeval timeout;
} server_t;

