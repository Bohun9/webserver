#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "server.h"
#include "common.h"
#include "http_handler.h"

static void reaper(int sig) {
    (void) sig;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        continue;
    }
    errno = saved_errno;
}

void server_init(server_t *server, uint16_t port, char *directory) {
    server->directory = directory;
    server->timeout.tv_sec = 1;
    server->timeout.tv_usec = 0;
    server->listen_fd = aver(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP), "socket");

    int enable = 1;
    aver(setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)), "setsockopt");

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.1.1");
    addr.sin_port = htons(port);

    aver(bind(server->listen_fd, (struct sockaddr *) &addr, sizeof(addr)), "bind");
    aver(listen(server->listen_fd, QUEUE_SIZE), "listen");

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = reaper;
    aver(sigaction(SIGCHLD, &sa, NULL), "sigaction");
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> <directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    uint16_t port = atoi(argv[1]);
    char *directory = argv[2];

    if (!exists(directory)) {
        fprintf(stderr, "'%s' argument does not exist.\n", directory);
        exit(EXIT_FAILURE);
    }
    if (!is_directory(directory)) {
        fprintf(stderr, "'%s' argument is not directory.\n", directory);
        exit(EXIT_FAILURE);
    }

    server_t server;
    server_init(&server, port, directory);

    debug_msg("[SERVER START] pid:%d fd:%d\n", getpid(), server.listen_fd);

    for (;;) {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        int connect_fd = aver(accept(server.listen_fd, (struct sockaddr *) &addr, &addr_len), "accept");

        switch (fork()) {
            case -1:
                aver(-1, "fork");
                exit(EXIT_FAILURE);
            case 0:
                close(server.listen_fd);
                handle_client(connect_fd, &server);
                exit(EXIT_SUCCESS);
            default:
                close(connect_fd);
        }
    }
}

