#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ip> <port> <username>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    uint16_t port = parse_port(argv[2]);
    const char *username = argv[3];

    if (port == 0) {
        return 1;
    }

    if (strlen(username) >= USER_SIZE) {
        fprintf(stderr, "Username is too long. Max %d characters.\n", USER_SIZE - 1);
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", ip);
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    Message intro;
    memset(&intro, 0, sizeof(intro));
    intro.type = MSG_FOLLOW;
    strncpy(intro.username, username, USER_SIZE - 1);

    if (send_all(sock, &intro, sizeof(intro)) < 0) {
        perror("send");
        close(sock);
        return 1;
    }

    printf("Conectado ao servidor como %s\n", username);

    char buffer[BUFFER_SIZE];
    uint32_t next_id = 1;

    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        buffer[strcspn(buffer, "\r\n")] = '\0';
        if (buffer[0] == '\0') {
            continue;
        }

        Message msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = MSG_POST;
        msg.msg_id = next_id++;
        strncpy(msg.username, username, USER_SIZE - 1);
        strncpy(msg.content, buffer, CONTENT_SIZE - 1);

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
