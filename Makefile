# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Iinclude
#CFLAGS = -Wall -Wextra -Iinclude

# Output Binary
TARGET = build/server

# Source Files
SRC = src/main.c src/client.c src/http.c src/network.c src/file_utils.c

OBJ = $(SRC:src/%.c=build/%.o)

# Default rule
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ # gcc -Wall -Iinclude -o build/server src/main.c src/client.c src/http.c src/network.c src/file_utils.c

