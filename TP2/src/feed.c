#include "feed.h"

#include <string.h>

void feed_init(Feed *feed) {
    feed->count = 0;
    feed->start = 0;
    memset(feed->entries, 0, sizeof(feed->entries));
}

void feed_add(Feed *feed, const FeedEntry *entry) {
    size_t index = 0;

    if (feed->count < FEED_CAPACITY) {
        index = (feed->start + feed->count) % FEED_CAPACITY;
        feed->count++;
    } else {
        index = feed->start;
        feed->start = (feed->start + 1) % FEED_CAPACITY;
    }

    feed->entries[index].msg_id = entry->msg_id;
    memcpy(feed->entries[index].username, entry->username, USER_SIZE);
    feed->entries[index].username[USER_SIZE - 1] = '\0';
    memcpy(feed->entries[index].content, entry->content, CONTENT_SIZE);
    feed->entries[index].content[CONTENT_SIZE - 1] = '\0';
}

size_t feed_count(const Feed *feed) {
    return feed->count;
}

const FeedEntry *feed_get(const Feed *feed, size_t index) {
    if (index >= feed->count) {
        return NULL;
    }

    size_t real_index = (feed->start + index) % FEED_CAPACITY;
    return &feed->entries[real_index];
}
