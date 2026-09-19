CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -Iinclude
LDFLAGS ?= -pthread

BIN_DIR := bin
SRC_DIR := src

.PHONY: all server client clean test

all: server client

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

server: $(BIN_DIR) $(BIN_DIR)/server
client: $(BIN_DIR) $(BIN_DIR)/client

$(BIN_DIR)/server: $(SRC_DIR)/server.c include/protocol.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/client: $(SRC_DIR)/client.c include/protocol.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# 本机同时起 1 个 Server + 3 个 Client，验证步骤 1、2
test: all
	bash scripts/run_three_clients.sh

clean:
	rm -rf $(BIN_DIR) /tmp/linux_tcp_lab_server.log
