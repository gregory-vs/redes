#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 16
#define BUFFER_SIZE 512

typedef struct {
    int client_fd;
    struct sockaddr_in addr;
} ClientArgs;

static int send_all(int fd, const char *buffer, size_t length) {
    size_t sent_total = 0;

    while (sent_total < length) {
        ssize_t sent = send(fd, buffer + sent_total, length - sent_total, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        sent_total += (size_t)sent;
    }

    return 0;
}

static void *client_thread(void *arg) {
    ClientArgs *client = (ClientArgs *)arg;
    int client_fd = client->client_fd;
    char address[INET_ADDRSTRLEN] = "unknown";
    uint16_t port = ntohs(client->addr.sin_port);

    inet_ntop(AF_INET, &client->addr.sin_addr, address, sizeof(address));
    free(client);

    printf("Client connected: %s:%u\n", address, port);

    const char *welcome = "Welcome! Type something and I will echo it back.\n";
    if (send_all(client_fd, welcome, strlen(welcome)) < 0) {
        perror("send");
        close(client_fd);
        return NULL;
    }

    for (;;) {
        char buffer[BUFFER_SIZE];
        ssize_t received = recv(client_fd, buffer, sizeof(buffer), 0);

        if (received == 0) {
            break;
        }

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv");
            break;
        }

        printf("Message from %s:%u: ", address, port);
        fwrite(buffer, 1, (size_t)received, stdout);
        if (buffer[received - 1] != '\n') {
            printf("\n");
        }
        fflush(stdout);

        if (send_all(client_fd, buffer, (size_t)received) < 0) {
            perror("send");
            break;
        }
    }

    close(client_fd);
    printf("Client disconnected: %s:%u\n", address, port);
    return NULL;
}

int server_run(uint16_t port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);
    printf("Server listening on port %u\n", port);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        ClientArgs *client = malloc(sizeof(*client));
        if (client == NULL) {
            perror("malloc");
            close(client_fd);
            continue;
        }

        client->client_fd = client_fd;
        client->addr = client_addr;

        pthread_t thread;
        if (pthread_create(&thread, NULL, client_thread, client) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(client);
            continue;
        }

        pthread_detach(thread);
    }

    close(server_fd);
    return 0;
}
