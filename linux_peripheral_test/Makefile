CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -g
LDFLAGS ?= -pthread
INCLUDES = -Iinclude -Isrc

BIN_DIR = build
COMMON  = src/common.c

.PHONY: all clean test

all: $(BIN_DIR)/server $(BIN_DIR)/client

$(BIN_DIR)/server: src/server.c $(COMMON) include/protocol.h src/common.h
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ src/server.c $(COMMON) $(LDFLAGS)

$(BIN_DIR)/client: src/client.c $(COMMON) include/protocol.h src/common.h
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ src/client.c $(COMMON) $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)

# 本机自测：不需要开发板，先验证 3 个 Client + 3 个线程
test: all
	bash scripts/self_test.sh
