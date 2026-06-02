#include "server.h"

#include "protocol.h"
#include "feed.h"
#include "followers.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 16
#define FEED_READ_LIMIT 5
#define ACTIVE_CLIENTS_MAX 64

typedef struct {
    int fd;
    char username[USER_SIZE];
} ActiveClient;

typedef struct {
    int client_fd;
    struct sockaddr_storage addr;
} ClientArgs;

static Feed feed;
static pthread_mutex_t feed_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t feed_next_id = 1;

static Followers followers;
static pthread_mutex_t followers_mutex = PTHREAD_MUTEX_INITIALIZER;

static ActiveClient active_clients[ACTIVE_CLIENTS_MAX];
static size_t active_clients_count = 0;
static pthread_mutex_t active_clients_mutex = PTHREAD_MUTEX_INITIALIZER;

static int send_all(int fd, const void *buffer, size_t length) {
    const char *data = buffer;
    size_t sent_total = 0;

    while (sent_total < length) {
        ssize_t sent = send(fd, data + sent_total, length - sent_total, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        sent_total += (size_t)sent;
    }

    return 0;
}

static int recv_all(int fd, void *buffer, size_t length) {
    char *data = buffer;
    size_t received_total = 0;

    while (received_total < length) {
        ssize_t received = recv(fd, data + received_total, length - received_total, 0);
        if (received == 0) {
            return 1;
        }
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        received_total += (size_t)received;
    }

    return 0;
}

static void message_to_network_order(Message *message) {
    message->type = htons(message->type);
    message->msg_id = htonl(message->msg_id);
}

static void message_to_host_order(Message *message) {
    message->type = ntohs(message->type);
    message->msg_id = ntohl(message->msg_id);
}

static int send_message(int fd, const Message *message) {
    Message network_message = *message;
    message_to_network_order(&network_message);
    return send_all(fd, &network_message, sizeof(network_message));
}

static void active_clients_add(int fd, const char *username) {
    pthread_mutex_lock(&active_clients_mutex);

    if (active_clients_count < ACTIVE_CLIENTS_MAX) {
        active_clients[active_clients_count].fd = fd;

        const char *normalized = username;
        while (*normalized == '@') {
            normalized++;
        }

        strncpy(active_clients[active_clients_count].username, normalized, USER_SIZE - 1);
        active_clients[active_clients_count].username[USER_SIZE - 1] = '\0';
        active_clients_count++;
    }

    pthread_mutex_unlock(&active_clients_mutex);
}

static void active_clients_remove(int fd) {
    pthread_mutex_lock(&active_clients_mutex);

    for (size_t i = 0; i < active_clients_count; i++) {
        if (active_clients[i].fd == fd) {
            active_clients[i] = active_clients[active_clients_count - 1];
            active_clients_count--;
            break;
        }
    }

    pthread_mutex_unlock(&active_clients_mutex);
}

static size_t bounded_strlen(const char *value, size_t max_len) {
    size_t len = 0;

    while (len < max_len && value[len] != '\0') {
        len++;
    }

    return len;
}

static FeedEntry store_post(const char *username, const Message *message) {
    FeedEntry entry;
    memset(&entry, 0, sizeof(entry));

    entry.msg_id = feed_next_id++;
    strncpy(entry.username, username, USER_SIZE - 1);
    entry.username[USER_SIZE - 1] = '\0';

    memcpy(entry.content, message->content, CONTENT_SIZE);
    entry.content[CONTENT_SIZE - 1] = '\0';

    feed_add(&feed, &entry);

    const char *prefix = (entry.username[0] == '@') ? "" : "@";
    size_t content_len = bounded_strlen(entry.content, CONTENT_SIZE);

    printf("[LOG] %s%s posted (ID %u): \"%.*s\"\n",
           prefix,
           entry.username,
           entry.msg_id,
           (int)content_len,
           entry.content);

    return entry;
}

static void send_push_to_followers(const FeedEntry *entry) {
    pthread_mutex_lock(&followers_mutex);

    const FollowersEntry *followers_entry = followers_get(&followers, entry->username);
    if (followers_entry == NULL) {
        pthread_mutex_unlock(&followers_mutex);
        return;
    }

    char followers_snapshot[FOLLOWERS_MAX_PER_USER][USER_SIZE];
    size_t follower_count = followers_entry->follower_count;

    for (size_t i = 0; i < follower_count; i++) {
        memcpy(followers_snapshot[i], followers_entry->followers[i], USER_SIZE);
    }

    pthread_mutex_unlock(&followers_mutex);

    Message push;
    memset(&push, 0, sizeof(push));

    push.type = MSG_PUSH;
    push.msg_id = entry->msg_id;
    memcpy(push.username, entry->username, USER_SIZE);
    memcpy(push.content, entry->content, CONTENT_SIZE);

    pthread_mutex_lock(&active_clients_mutex);

    for (size_t i = 0; i < active_clients_count; i++) {
        for (size_t j = 0; j < follower_count; j++) {
            if (strncmp(active_clients[i].username, followers_snapshot[j], USER_SIZE) == 0) {
                send_message(active_clients[i].fd, &push);
                break;
            }
        }
    }

    pthread_mutex_unlock(&active_clients_mutex);
}

static size_t feed_snapshot(FeedEntry *entries, size_t max_entries) {
    size_t total = 0;
    size_t start = 0;

    pthread_mutex_lock(&feed_mutex);

    total = feed_count(&feed);
    if (total > max_entries) {
        start = total - max_entries;
    }

    size_t copied = 0;
    for (size_t i = start; i < total; i++) {
        const FeedEntry *entry = feed_get(&feed, i);
        if (entry != NULL) {
            entries[copied++] = *entry;
        }
    }

    pthread_mutex_unlock(&feed_mutex);

    return copied;
}

static int send_feed_entries(int client_fd) {
    FeedEntry entries[FEED_READ_LIMIT];
    size_t count = feed_snapshot(entries, FEED_READ_LIMIT);

    for (size_t i = count; i > 0; i--) {
        const FeedEntry *entry = &entries[i - 1];

        Message response;
        memset(&response, 0, sizeof(response));

        response.type = MSG_PUSH;
        response.msg_id = entry->msg_id;

        memcpy(response.username, entry->username, USER_SIZE);
        response.username[USER_SIZE - 1] = '\0';

        memcpy(response.content, entry->content, CONTENT_SIZE);
        response.content[CONTENT_SIZE - 1] = '\0';

        if (send_message(client_fd, &response) < 0) {
            return -1;
        }
    }

    Message end;
    memset(&end, 0, sizeof(end));
    end.type = MSG_END;

    if (send_message(client_fd, &end) < 0) {
        return -1;
    }

    return 0;
}

static int handle_message(int client_fd, const char *username, const Message *message) {
    if (message->type == MSG_POST) {
        pthread_mutex_lock(&feed_mutex);
        FeedEntry entry = store_post(username, message);
        pthread_mutex_unlock(&feed_mutex);

        send_push_to_followers(&entry);
        return 0;
    }

    if (message->type == MSG_FOLLOW) {
        pthread_mutex_lock(&followers_mutex);
        followers_add(&followers, message->content, username);
        pthread_mutex_unlock(&followers_mutex);
        return 0;
    }

    if (message->type == MSG_READ) {
        return send_feed_entries(client_fd);
    }

    return 0;
}

static void *client_thread(void *arg) {
    ClientArgs *client = (ClientArgs *)arg;
    int client_fd = client->client_fd;
    char username[USER_SIZE + 1];

    free(client);

    Message message;
    int status = recv_all(client_fd, &message, sizeof(message));
    if (status != 0) {
        close(client_fd);
        return NULL;
    }

    message_to_host_order(&message);

    memcpy(username, message.username, USER_SIZE);
    username[USER_SIZE] = '\0';

    printf("[CONN] %s%s conectou.\n",
           (username[0] == '@') ? "" : "@",
           username);

    active_clients_add(client_fd, username);

    if (message.type != MSG_CONNECT) {
        if (handle_message(client_fd, username, &message) < 0) {
            perror("send");
            active_clients_remove(client_fd);
            close(client_fd);
            return NULL;
        }
    }

    for (;;) {
        status = recv_all(client_fd, &message, sizeof(message));

        if (status == 1) {
            break;
        }

        if (status < 0) {
            perror("recv");
            break;
        }

        message_to_host_order(&message);

        if (handle_message(client_fd, username, &message) < 0) {
            perror("send");
            break;
        }
    }

    active_clients_remove(client_fd);
    close(client_fd);

    printf("[DISC] %s%s desconectou.\n",
           (username[0] == '@') ? "" : "@",
           username);

    return NULL;
}

int server_run(ServerProtocol protocol, uint16_t port) {
    feed_init(&feed);
    followers_init(&followers);

    int family = (protocol == SERVER_PROTOCOL_V4) ? AF_INET : AF_INET6;
    int server_fd = socket(family, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    if (protocol == SERVER_PROTOCOL_V6) {
        int v6only = 1;
        if (setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) < 0) {
            perror("setsockopt IPV6_V6ONLY");
            close(server_fd);
            return 1;
        }
    }

    if (protocol == SERVER_PROTOCOL_V4) {
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));

        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("bind");
            close(server_fd);
            return 1;
        }
    } else {
        struct sockaddr_in6 server_addr;
        memset(&server_addr, 0, sizeof(server_addr));

        server_addr.sin6_family = AF_INET6;
        server_addr.sin6_addr = in6addr_any;
        server_addr.sin6_port = htons(port);

        if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("bind");
            close(server_fd);
            return 1;
        }
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    printf("Aguardando conexoes na porta %u.\n", port);

    for (;;) {
        struct sockaddr_storage client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("accept");
            continue;
        }

        ClientArgs *client = malloc(sizeof(*client));
        if (client == NULL) {
            perror("malloc");
            close(client_fd);
            continue;
        }

        client->client_fd = client_fd;
        client->addr = client_addr;

        pthread_t thread;
        if (pthread_create(&thread, NULL, client_thread, client) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(client);
            continue;
        }

        pthread_detach(thread);
    }

    close(server_fd);
    return 0;
}