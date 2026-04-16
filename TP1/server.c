#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "protocol.h"

int main(int argc, char *argv[]) {

    if (argc != 4) {
        printf("Uso: ./server <v4|v6> <porta> <senha>\n");
        exit(1);
    }

    char *protocol = argv[1];
    int port = atoi(argv[2]);
    char *secret = argv[3];
    int attempts = 0;

    int server_sock;

    if (strcmp(protocol, "v4") == 0) {
        server_sock = socket(AF_INET, SOCK_STREAM, 0);
    } else if (strcmp(protocol, "v6") == 0) {
        server_sock = socket(AF_INET6, SOCK_STREAM, 0);
    } else {
        printf("Protocolo inválido. Use v4 ou v6\n");
        exit(1);
    }

    if (server_sock < 0) {
        perror("Erro ao criar socket");
        exit(1);
    }

    if (strcmp(protocol, "v4") == 0) {

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));

        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Erro no bind");
            close(server_sock);
            exit(1);
        }

        printf("Servidor iniciado em modo IPv4 na porta %d\n", port);

    } 
    
    if (strcmp(protocol, "v6") == 0) {

        struct sockaddr_in6 server_addr6;
        memset(&server_addr6, 0, sizeof(server_addr6));

        server_addr6.sin6_family = AF_INET6;
        server_addr6.sin6_addr = in6addr_any;
        server_addr6.sin6_port = htons(port);

        if (bind(server_sock, (struct sockaddr*)&server_addr6, sizeof(server_addr6)) < 0) {
            perror("Erro no bind");
            close(server_sock);
            exit(1);
        }

        printf("Servidor iniciado em modo IPv6 na porta %d\n", port);
    }

    if (listen(server_sock, 1) < 0) {
        perror("Erro no listen");
        close(server_sock);
        exit(1);
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);

    if (client_sock < 0) {
        perror("Erro no accept");
        close(server_sock);
        exit(1);
    }

    printf("Cliente conectado\n");

    HackerMessage msg;
    memset(&msg, 0, sizeof(HackerMessage));

    msg.type = MSG_START;
    msg.attempts = 0;
    msg.win_status = 0;

    strncpy(msg.message, "START", MSG_SIZE);

    if (send(client_sock, &msg, sizeof(HackerMessage), 0) < 0) {
        perror("Erro no send");
    }

    while (1) {

        HackerMessage recv_msg;
        memset(&recv_msg, 0, sizeof(HackerMessage));

        int bytes = recv(client_sock, &recv_msg, sizeof(HackerMessage), 0);

        if (bytes <= 0) {
            printf("Cliente desconectado\n");
            break;
        }

        if (recv_msg.type == MSG_GUESS) {

            attempts++;

            char guess_str[6];
            for (int i = 0; i < 5; i++) {
                guess_str[i] = recv_msg.guess[i] + '0';
            }
            guess_str[5] = '\0';

            HackerMessage resp;
            memset(&resp, 0, sizeof(HackerMessage));

            resp.attempts = attempts;

            if (strcmp(guess_str, secret) == 0) {
                resp.type = MSG_WIN;
                resp.win_status = 1;

                send(client_sock, &resp, sizeof(HackerMessage), 0);
                break;

            } else {
                resp.type = MSG_FEEDBACK;
                resp.win_status = 0;

                send(client_sock, &resp, sizeof(HackerMessage), 0);
            }
        }
    }

    close(client_sock);
    close(server_sock);

    return 0;
}
