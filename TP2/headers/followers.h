#ifndef FOLLOWERS_H
#define FOLLOWERS_H

#include <stddef.h>

#include "protocol.h"

#define FOLLOWERS_MAX_USERS 64
#define FOLLOWERS_MAX_PER_USER 64

typedef struct {
    char username[USER_SIZE];
    char followers[FOLLOWERS_MAX_PER_USER][USER_SIZE];
    size_t follower_count;
} FollowersEntry;

typedef struct {
    FollowersEntry entries[FOLLOWERS_MAX_USERS];
    size_t count;
} Followers;

void followers_init(Followers *followers);
int followers_add(Followers *followers, const char *user, const char *follower);
const FollowersEntry *followers_get(const Followers *followers, const char *user);

#endif
