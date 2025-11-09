# ===== Makefile =====
CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -pthread
LDFLAGS := -pthread

EXTERNAL_DIR := mnt/data

# Project layout
SRCDIR := src
SHARED := $(SRCDIR)/shared
SERVER := $(SRCDIR)/server
CLIENT := $(SRCDIR)/client

# Build output directories
BUILD      := build
OBJDIR     := $(BUILD)/obj
OBJ_SERVER := $(OBJDIR)/server
OBJ_CLIENT := $(OBJDIR)/client
OBJ_SHARED := $(OBJDIR)/shared
OBJ_EXT    := $(OBJDIR)/external

# Sources
SERVER_SRCS := $(SERVER)/main.c $(SERVER)/client_handler.c
CLIENT_SRCS := $(CLIENT)/main.c $(CLIENT)/receiver_handler.c $(CLIENT)/sender_handler.c
SHARED_SRCS := $(SHARED)/message.c $(SHARED)/chat_node.c
EXT_SRCS    := $(EXTERNAL_DIR)/properties.c

# Objects (mirror into build/obj/...)
SERVER_OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SERVER_SRCS))
CLIENT_OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(CLIENT_SRCS))
SHARED_OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SHARED_SRCS))
EXT_OBJS    := $(patsubst $(EXTERNAL_DIR)/%.c,$(OBJ_EXT)/%.o,$(EXT_SRCS))

# Final binaries
SERVER_BIN := $(BUILD)/chat_server
CLIENT_BIN := $(BUILD)/chat_client

.PHONY: all clean dirs

all: dirs $(SERVER_BIN) $(CLIENT_BIN)

dirs:
	@mkdir -p $(OBJ_SERVER) $(OBJ_CLIENT) $(OBJ_SHARED) $(OBJ_EXT) $(BUILD)

# Binaries
$(SERVER_BIN): $(SERVER_OBJS) $(SHARED_OBJS) $(EXT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_OBJS) $(SHARED_OBJS) $(EXT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile project sources -> build/obj/...
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(EXTERNAL_DIR) -c -o $@ $<

# Compile external properties.c -> build/obj/external/...
$(OBJ_EXT)/%.o: $(EXTERNAL_DIR)/%.c $(EXTERNAL_DIR)/%.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(EXTERNAL_DIR) -c -o $@ $<

clean:
	rm -rf $(BUILD)
