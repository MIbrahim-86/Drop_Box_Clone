# Makefile for Dropbox Server

CC = gcc
# Default flags for debugging
CFLAGS = -g -Wall -pthread

# Executable name
TARGET = dropbox_server
CLIENT = dropbox_client

# Source files
SRCS = server.c threadpool.c queue.c user_auth.c file_ops.c response_queue.c lock_manager.c
CLIENT_SRCS = client.c

# Object files
OBJS = $(SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

.PHONY: all clean memcheck racecheck

all: $(TARGET) $(CLIENT)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(CLIENT): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $(CLIENT) $(CLIENT_OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(CLIENT) $(OBJS) $(CLIENT_OBJS)

# Rule to run server with Valgrind (for memory leaks)
memcheck: CFLAGS = -g -Wall -pthread
memcheck: clean all
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

# Rule to run server (compiled with -fsanitize=thread for data races)
racecheck: CFLAGS = -g -Wall -pthread -fsanitize=thread
racecheck: clean all
	./$(TARGET)