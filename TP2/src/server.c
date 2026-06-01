#include "server.h"

#include "protocol.h"

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

typedef struct {
    int client_fd;
    struct sockaddr_storage addr;
} ClientArgs;

static int send_all(int fd, const void *buffer, size_t length) {
    const char *data = buffer;
    size_t sent_total = 0;

    while (sent_total < length) {
        ssize_t sent = send(fd, data + sent_total, length - sent_total, 0);
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

static int recv_all(int fd, void *buffer, size_t length) {
    char *data = buffer;
    size_t received_total = 0;

    while (received_total < length) {
        ssize_t received = recv(fd, data + received_total, length - received_total, 0);
        if (received == 0) {
            return 1;
        }
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        received_total += (size_t)received;
    }

    return 0;
}

static const char *message_type_name(uint16_t type) {
    switch (type) {
        case MSG_POST:
            return "POST";
        case MSG_FOLLOW:
            return "FOLLOW";
        case MSG_READ:
            return "READ";
        case MSG_PUSH:
            return "PUSH";
        default:
            return "UNKNOWN";
    }
}

static void print_selection(const char *username, const Message *message) {
    printf("%s selected %s\n", username, message_type_name(message->type));
}

static void *client_thread(void *arg) {
    ClientArgs *client = (ClientArgs *)arg;
    int client_fd = client->client_fd;
    char address[INET6_ADDRSTRLEN] = "unknown";
    uint16_t port = 0;
    char username[USER_SIZE + 1];

    if (client->addr.ss_family == AF_INET) {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)&client->addr;
        inet_ntop(AF_INET, &addr4->sin_addr, address, sizeof(address));
        port = ntohs(addr4->sin_port);
    } else if (client->addr.ss_family == AF_INET6) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&client->addr;
        inet_ntop(AF_INET6, &addr6->sin6_addr, address, sizeof(address));
        port = ntohs(addr6->sin6_port);
    }
    free(client);

    Message message;
    int status = recv_all(client_fd, &message, sizeof(message));
    if (status != 0) {
        close(client_fd);
        return NULL;
    }

    memcpy(username, message.username, USER_SIZE);
    username[USER_SIZE] = '\0';
    printf("[CONN] %s conectou.\n", username);

    print_selection(username, &message);
    if (send_all(client_fd, &message, sizeof(message)) < 0) {
        perror("send");
        close(client_fd);
        return NULL;
    }

    for (;;) {
        status = recv_all(client_fd, &message, sizeof(message));
        if (status == 1) {
            break;
        }
        if (status < 0) {
            perror("recv");
            break;
        }

        print_selection(username, &message);
        if (send_all(client_fd, &message, sizeof(message)) < 0) {
            perror("send");
            break;
        }
    }

    close(client_fd);
    printf("Client disconnected: %s:%u\n", address, port);
    return NULL;
}

int server_run(ServerProtocol protocol, uint16_t port) {
    int family = (protocol == SERVER_PROTOCOL_V4) ? AF_INET : AF_INET6;
    int server_fd = socket(family, SOCK_STREAM, 0);
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

    if (protocol == SERVER_PROTOCOL_V6) {
        int v6only = 1;
        if (setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) < 0) {
            perror("setsockopt IPV6_V6ONLY");
            close(server_fd);
            return 1;
        }
    }

    if (protocol == SERVER_PROTOCOL_V4) {
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
    } else {
        struct sockaddr_in6 server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin6_family = AF_INET6;
        server_addr.sin6_addr = in6addr_any;
        server_addr.sin6_port = htons(port);

        if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("bind");
            close(server_fd);
            return 1;
        }
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);
    printf("Aguardando conexoes na porta %u\n", port);

    for (;;) {
        struct sockaddr_storage client_addr;
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
