CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -std=c11
INCLUDES = -Iheaders

OBJ_DIR = obj
TARGET = app

SRCS = main.c src/protocol.c
OBJS = $(SRCS:%.c=$(OBJ_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean
