CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -std=c11 -pthread
INCLUDES = -Iheaders

OBJ_DIR = obj
SERVER_TARGET = server
CLIENT_TARGET = client

SERVER_SRCS = main.c src/protocol.c src/server.c
CLIENT_SRCS = client.c
SERVER_OBJS = $(SERVER_SRCS:%.c=$(OBJ_DIR)/%.o)
CLIENT_OBJS = $(CLIENT_SRCS:%.c=$(OBJ_DIR)/%.o)

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CC) $(CFLAGS) $(SERVER_OBJS) -o $@

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $(CLIENT_OBJS) -o $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(SERVER_TARGET) $(CLIENT_TARGET)

.PHONY: all clean
