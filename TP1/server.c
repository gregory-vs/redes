#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "protocol.h"

int validate_pass(char *attempt, char *correct_pass, int feedback[5]);

int main(int argc, char *argv[]) {

    if (argc != 4) {
        printf("Uso: ./server <v4|v6> <porta> <senha>\n");
        exit(1);
    }

    char *protocol = argv[1];
    int port = atoi(argv[2]);
    char *secret = argv[3];
    int attempts = 0;
    int feedback[5] = {0,0,0,0,0};

    int is_ipv4 = strcmp(protocol, "v4") == 0;

    int server_sock;

    if (is_ipv4) {
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

    struct sockaddr_storage server_addr;
    socklen_t addr_len;

    memset(&server_addr, 0, sizeof(server_addr));

    if (is_ipv4) {
        struct sockaddr_in *addr4 = (struct sockaddr_in*)&server_addr;

        addr4->sin_family = AF_INET;
        addr4->sin_addr.s_addr = INADDR_ANY;
        addr4->sin_port = htons(port);

        addr_len = sizeof(struct sockaddr_in);
    } else {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6*)&server_addr;

        addr6->sin6_family = AF_INET6;
        addr6->sin6_addr = in6addr_any;
        addr6->sin6_port = htons(port);

        addr_len = sizeof(struct sockaddr_in6);
    }

    if (bind(server_sock, (struct sockaddr*)&server_addr, addr_len) < 0) {
        perror("Erro no bind");
        close(server_sock);
        exit(1);
    }

    if (listen(server_sock, 1) < 0) {
        perror("Erro no listen");
        close(server_sock);
        exit(1);
    }

    if (is_ipv4) {
        printf("Servidor iniciado em modo IPv4 na porta %d\n", port);
    } else {
        printf("Servidor iniciado em modo IPv6 na porta %d\n", port);
    }

    struct sockaddr_storage client_addr;
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

    HackerMessage resp;
    memset(&resp, 0, sizeof(HackerMessage));

    while (1) {

        HackerMessage recv_msg;
        memset(&recv_msg, 0, sizeof(HackerMessage));

        int bytes = recv(client_sock, &recv_msg, sizeof(HackerMessage), 0);

        if (recv_msg.type == MSG_GUESS) {

            memset(&resp, 0, sizeof(HackerMessage));

            char guess_str[6];
            for (int i = 0; i < 5; i++) {
                guess_str[i] = recv_msg.guess[i] + '0';
            }
            guess_str[5] = '\0';

            if (validate_pass(guess_str, secret, feedback) == 0) {
                resp.type = MSG_ERROR;
                resp.win_status = -1;
                resp.attempts = attempts;
                strncpy(resp.message, "Insira uma sequ^encia válida!", MSG_SIZE);
            } else {
                attempts++;
                resp.attempts = attempts;

                if (strcmp(guess_str, secret) == 0) {
                    resp.type = MSG_WIN;
                    resp.win_status = 1;
                } else {
                    resp.type = MSG_FEEDBACK;
                    resp.win_status = 0;
                }

                for (int i = 0; i < 5; i++) {
                    resp.feedback[i] = feedback[i];
                }
            }

            send(client_sock, &resp, sizeof(HackerMessage), 0);

            if (resp.type == MSG_WIN) {
                break;
            }
        }
    }

    close(client_sock);
    close(server_sock);

    printf("Cliente desconectado\n");
    return 0;
}

int validate_pass(char *attempt, char *correct_pass, int feedback[5]) {
    if (strlen(attempt) != 5) {
        return 0;
    }

    for (int i = 0; i < 5; i++) {
        if (attempt[i] < '0' || attempt[i] > '9') {
            return 0;
        }
    }

    for (int i = 0; i < 5; i++) {
        feedback[i] = 0;
    }
    int used_secret[5] = {0,0,0,0,0};

    for(int i = 0; i < 5; i++) {
        if (attempt[i] == correct_pass[i]) {
            feedback[i] = 2;
            used_secret[i] = 1;
        }
    }

    for(int i=0; i<5; i++) {
        if (feedback[i] != 2) {
            for (int j = 0; j < 5; j++) {
                if (attempt[i] == correct_pass[j] && used_secret[j] == 0) { 
                    feedback[i] = 1;
                    used_secret[j] = 1;
                    break;
                }
            }
        }
    }
    return 1;
}