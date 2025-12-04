# got tired of writing this myself. this is more robust

# ---------------------------------------------------------
# CONFIG
# ---------------------------------------------------------

CC      = gcc
CFLAGS  = -Iinclude -I/usr/include/libnl3
LDFLAGS = -lnl-3 -lnl-genl-3 -lnl-route-3 -lpthread -lcrypto

# Utility modules inside ./util/
UTILS = VPNcrypt VPNnetwork VPNtools
UTIL_SRC = $(addprefix util/, $(addsuffix .c, $(UTILS)))
UTIL_OBJ = $(addprefix build/, $(addsuffix .o, $(UTILS)))

# Output directories
BUILD_DIR = build
BIN_DIR   = bin

# Main programs
CLIENT_SRC = VPNclient.c
SERVER_SRC = VPNserver.c

CLIENT_OBJ = $(BUILD_DIR)/VPNclient.o
SERVER_OBJ = $(BUILD_DIR)/VPNserver.o

CLIENT_BIN = $(BIN_DIR)/VPNclient.out
SERVER_BIN = $(BIN_DIR)/VPNserver.out

# ---------------------------------------------------------
# DEFAULT
# ---------------------------------------------------------

all: client server

# ---------------------------------------------------------
# BUILD TARGETS
# ---------------------------------------------------------

client: $(CLIENT_BIN)
server: $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_OBJ) $(UTIL_OBJ) | $(BIN_DIR)
	$(CC) $(CLIENT_OBJ) $(UTIL_OBJ) -o $@ $(LDFLAGS)

$(SERVER_BIN): $(SERVER_OBJ) $(UTIL_OBJ) | $(BIN_DIR)
	$(CC) $(SERVER_OBJ) $(UTIL_OBJ) -o $@ $(LDFLAGS)

# ---------------------------------------------------------
# COMPILATION RULES
# ---------------------------------------------------------

# Rule for compiling utility modules from util/*.c → build/*.o
build/%.o: util/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for compiling client/server from root directory
build/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ---------------------------------------------------------
# DIRECTORY CREATION
# ---------------------------------------------------------

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# ---------------------------------------------------------
# CLEAN
# ---------------------------------------------------------

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all client server clean