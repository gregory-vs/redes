#ifndef FEED_H
#define FEED_H

#include <stddef.h>
#include <stdint.h>

#include "protocol.h"

#define FEED_CAPACITY 20

typedef struct {
    uint32_t msg_id;
    char username[USER_SIZE];
    char content[CONTENT_SIZE];
} FeedEntry;

typedef struct {
    FeedEntry entries[FEED_CAPACITY];
    size_t count;
    size_t start;
} Feed;

void feed_init(Feed *feed);
void feed_add(Feed *feed, const FeedEntry *entry);
size_t feed_count(const Feed *feed);
const FeedEntry *feed_get(const Feed *feed, size_t index);

#endif
