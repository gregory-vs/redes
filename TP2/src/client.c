#define _POSIX_C_SOURCE 200112L

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol.h"

#define BUFFER_SIZE 512

static uint16_t parse_port(const char *value) {
    char *end = NULL;
    unsigned long port = strtoul(value, &end, 10);

    if (value[0] == '\0' || (end != NULL && *end != '\0') || port == 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", value);
        return 0;
    }

    return (uint16_t)port;
}

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

static size_t bounded_strlen(const char *value, size_t max_len) {
    size_t len = 0;

    while (len < max_len && value[len] != '\0') {
        len++;
    }

    return len;
}

static char *skip_spaces(char *value) {
    while (*value == ' ' || *value == '\t') {
        value++;
    }
    return value;
}

static int parse_command_line(char *line, Message *msg) {
    char *command = strtok(line, " \t");
    if (command == NULL) {
        return -1;
    }

    char *argument = strtok(NULL, "");
    if (argument != NULL) {
        argument = skip_spaces(argument);
    }

    if (strcasecmp(command, "FOLLOW") == 0) {
        msg->type = MSG_FOLLOW;
        if (argument == NULL || *argument == '\0') {
            fprintf(stderr, "Uso: FOLLOW <usuario>\n");
            return -1;
        }
        strncpy(msg->content, argument, CONTENT_SIZE - 1);
        return 0;
    }

    if (strcasecmp(command, "POST") == 0) {
        msg->type = MSG_POST;
        if (argument == NULL || *argument == '\0') {
            fprintf(stderr, "Uso: POST <mensagem>\n");
            return -1;
        }
        strncpy(msg->content, argument, CONTENT_SIZE - 1);
        return 0;
    }

    if (strcasecmp(command, "READ") == 0) {
        msg->type = MSG_READ;
        return 0;
    }

    fprintf(stderr, "Comando invalido: %s\n", command);
    return -1;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ip> <port> <username>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    const char *port_str = argv[2];
    uint16_t port = parse_port(port_str);
    const char *username = argv[3];

    if (port == 0) {
        return 1;
    }

    if (strlen(username) >= USER_SIZE) {
        fprintf(stderr, "Username is too long. Max %d characters.\n", USER_SIZE - 1);
        return 1;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result = NULL;
    int err = getaddrinfo(ip, port_str, &hints, &result);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return 1;
    }

    int sock = -1;
    int last_errno = 0;
    for (struct addrinfo *rp = result; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) {
            last_errno = errno;
            continue;
        }

        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }

        last_errno = errno;
        close(sock);
        sock = -1;
    }

    freeaddrinfo(result);

    if (sock < 0) {
        if (last_errno != 0) {
            errno = last_errno;
        }
        perror("connect");
        return 1;
    }

    printf("Conectado ao servidor como %s\n", username);

    char buffer[BUFFER_SIZE];
    while (1) {
        printf("Comandos: FOLLOW <usuario>, POST <mensagem>, READ\n");
        printf("> ");
        fflush(stdout);

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }
        buffer[strcspn(buffer, "\r\n")] = '\0';
        if (buffer[0] == '\0') {
            continue;
        }

        Message msg;
        memset(&msg, 0, sizeof(msg));
        if (parse_command_line(buffer, &msg) != 0) {
            continue;
        }
        strncpy(msg.username, username, USER_SIZE - 1);

        if (send_all(sock, &msg, sizeof(msg)) < 0) {
            perror("send");
            break;
        }

        Message response;
        int status = recv_all(sock, &response, sizeof(response));
        if (status == 1) {
            break;
        }
        if (status < 0) {
            perror("recv");
            break;
        }

        if (response.type == MSG_POST || response.type == MSG_PUSH) {
            size_t content_len = bounded_strlen(response.content, CONTENT_SIZE);
            fwrite(response.content, 1, content_len, stdout);
            if (content_len == 0 || response.content[content_len - 1] != '\n') {
                printf("\n");
            }
        }
    }

    close(sock);
    return 0;
}
