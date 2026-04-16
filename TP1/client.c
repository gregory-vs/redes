#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "protocol.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: ./client <ip> <porta>\n");
        exit(1);
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);

    int sock;

    struct sockaddr_storage server_addr;
    socklen_t addr_len;

    memset(&server_addr, 0, sizeof(server_addr));

    if (strchr(ip, ':') != NULL) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6*)&server_addr;

        sock = socket(AF_INET6, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("Erro ao criar socket");
            exit(1);
        }

        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(port);

        if (inet_pton(AF_INET6, ip, &addr6->sin6_addr) <= 0) {
            perror("IPv6 inválido");
            close(sock);
            exit(1);
        }

        addr_len = sizeof(struct sockaddr_in6);

    } else {
        struct sockaddr_in *addr4 = (struct sockaddr_in*)&server_addr;

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("Erro ao criar socket");
            exit(1);
        }

        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(port);

        if (inet_pton(AF_INET, ip, &addr4->sin_addr) <= 0) {
            perror("IPv4 inválido");
            close(sock);
            exit(1);
        }

        addr_len = sizeof(struct sockaddr_in);
    }

    if (connect(sock, (struct sockaddr*)&server_addr, addr_len) < 0) {
        perror("Erro ao conectar");
        close(sock);
        exit(1);
    }

    HackerMessage msg;
    memset(&msg, 0, sizeof(HackerMessage));

    int bytes = recv(sock, &msg, sizeof(HackerMessage), 0);

    if (bytes < 0) {
        perror("Erro no recv");
        close(sock);
        exit(1);
    } else if (bytes == 0) {
        printf("Servidor fechou a conexão\n");
        close(sock);
        exit(1);
    }

    if (msg.type == MSG_START) {
        printf("Insira seu palpite:\n");
    }

    close(sock);

    return 0;
}
