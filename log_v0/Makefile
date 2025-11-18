# Makefile for MCU Log SDK

CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -Os
LDFLAGS =

# Source files
SRC = src/log_str.c src/log_encode.c src/log_filter.c src/log_storage.c src/file_ids.c
MODULE_SRC = modules/driver/uart_driver.c modules/app/main_app.c
TEST_SRC = test/log_test.c

# Object files
OBJ = $(SRC:.c=.o) $(MODULE_SRC:.c=.o) $(TEST_SRC:.c=.o)

# Executable
TARGET = build/log_test.exe

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean