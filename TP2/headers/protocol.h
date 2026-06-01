#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define CONTENT_SIZE 140
#define USER_SIZE 16

typedef enum {
    MSG_CONNECT = 0,
    MSG_POST = 1,
    MSG_FOLLOW = 2,
    MSG_READ = 3,
    MSG_PUSH = 4,
    MSG_END = 5
} MessageType;

typedef struct {
    uint16_t type;
    char username[USER_SIZE];
    char content[CONTENT_SIZE];
    uint32_t msg_id;
} Message;

#endif
