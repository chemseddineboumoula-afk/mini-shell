CC      := gcc
CFLAGS  := -Wall -Wextra -Wpedantic -std=c11 -Iinclude
LDFLAGS :=

TARGET  := mini-shell
SRC_DIR := src
OBJ_DIR := build

SRCS    := $(wildcard $(SRC_DIR)/*.c)
OBJS    := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c include/shell.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	@set -e; \
	for t in tests/test_basic.sh tests/test_parser.sh tests/test_redirection.sh tests/test_pipes.sh; do \
		echo "=== Running $$t ==="; \
		bash "$$t"; \
	done; \
	echo "All test suites passed!"

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
