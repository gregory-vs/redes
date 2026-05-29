#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <ip> <port> <username>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    uint16_t port = parse_port(argv[2]);
    const char *username = argv[3];

    (void)username;

    if (port == 0) {
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

    char buffer[BUFFER_SIZE];
    ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (received > 0) {
        buffer[received] = '\0';
        fputs(buffer, stdout);
    }

    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        size_t len = strlen(buffer);

        if (len == 0) {
            continue;
        }

        if (send_all(sock, buffer, len) < 0) {
            perror("send");
            break;
        }

        received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received == 0) {
            break;
        }
        if (received < 0) {
            perror("recv");
            break;
        }

        buffer[received] = '\0';
        fputs(buffer, stdout);
    }

    close(sock);
    return 0;
}
