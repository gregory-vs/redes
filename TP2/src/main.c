#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "server.h"

static uint16_t parse_port(const char *value) {
    char *end = NULL;
    unsigned long port = strtoul(value, &end, 10);

    if (value[0] == '\0' || (end != NULL && *end != '\0') || port == 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", value);
        return 0;
    }

    return (uint16_t)port;
}

static int parse_protocol(const char *value, ServerProtocol *protocol) {
    if (strcmp(value, "v4") == 0) {
        *protocol = SERVER_PROTOCOL_V4;
        return 0;
    }
    if (strcmp(value, "v6") == 0) {
        *protocol = SERVER_PROTOCOL_V6;
        return 0;
    }

    fprintf(stderr, "Invalid protocol: %s (use v4 or v6)\n", value);
    return -1;
}

int main(int argc, char **argv) {
    uint16_t port = 0;
    ServerProtocol protocol;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <v4|v6> <port>\n", argv[0]);
        return 1;
    }

    if (parse_protocol(argv[1], &protocol) != 0) {
        return 1;
    }

    port = parse_port(argv[2]);
    if (port == 0) {
        return 1;
    }

    return server_run(protocol, port);
}
