#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>

typedef enum {
    SERVER_PROTOCOL_V4,
    SERVER_PROTOCOL_V6
} ServerProtocol;

int server_run(ServerProtocol protocol, uint16_t port);

#endif
