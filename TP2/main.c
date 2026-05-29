#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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

int main(int argc, char **argv) {
    uint16_t port = 8080;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        return 1;
    }

    if (argc == 2) {
        port = parse_port(argv[1]);
        if (port == 0) {
            return 1;
        }
    }

    return server_run(port);
}
