#include "followers.h"

#include <string.h>

static void normalize_username(const char *input, char *output) {
    if (input == NULL) {
        output[0] = '\0';
        return;
    }

    while (*input == '@') {
        input++;
    }

    strncpy(output, input, USER_SIZE - 1);
    output[USER_SIZE - 1] = '\0';
}

static int find_user_index(const Followers *followers, const char *username) {
    for (size_t i = 0; i < followers->count; i++) {
        if (strncmp(followers->entries[i].username, username, USER_SIZE) == 0) {
            return (int)i;
        }
    }

    return -1;
}

void followers_init(Followers *followers) {
    followers->count = 0;
    memset(followers->entries, 0, sizeof(followers->entries));
}

const FollowersEntry *followers_get(const Followers *followers, const char *user) {
    char normalized_user[USER_SIZE];
    normalize_username(user, normalized_user);

    int index = find_user_index(followers, normalized_user);
    if (index < 0) {
        return NULL;
    }

    return &followers->entries[index];
}

int followers_add(Followers *followers, const char *user, const char *follower) {
    char normalized_user[USER_SIZE];
    char normalized_follower[USER_SIZE];
    normalize_username(user, normalized_user);
    normalize_username(follower, normalized_follower);

    if (normalized_user[0] == '\0' || normalized_follower[0] == '\0') {
        return -1;
    }

    if (strncmp(normalized_user, normalized_follower, USER_SIZE) == 0) {
        return 1;
    }

    int index = find_user_index(followers, normalized_user);
    if (index < 0) {
        if (followers->count >= FOLLOWERS_MAX_USERS) {
            return -1;
        }
        index = (int)followers->count++;
        FollowersEntry *entry = &followers->entries[index];
        memset(entry, 0, sizeof(*entry));
        strncpy(entry->username, normalized_user, USER_SIZE - 1);
        entry->username[USER_SIZE - 1] = '\0';
    }

    FollowersEntry *entry = &followers->entries[index];
    for (size_t i = 0; i < entry->follower_count; i++) {
        if (strncmp(entry->followers[i], normalized_follower, USER_SIZE) == 0) {
            return 1;
        }
    }

    if (entry->follower_count >= FOLLOWERS_MAX_PER_USER) {
        return -1;
    }

    strncpy(entry->followers[entry->follower_count], normalized_follower, USER_SIZE - 1);
    entry->followers[entry->follower_count][USER_SIZE - 1] = '\0';
    entry->follower_count++;

    return 0;
}
