# Compilatore e flag di compilazione
CC = gcc
CFLAGS = -Wall -std=c11 -D_POSIX_C_SOURCE=200809L

# Nomi degli eseguibili
SERVER_BIN = server_app
CLIENT_BIN = client_app

# File sorgenti (common)
COMMON_SRC = common/message.c common/net_utils.c
COMMON_OBJ = $(COMMON_SRC:.c=.o)

# File sorgenti Server
SERVER_SRC = server/server_main.c server/handlers.c server/client_store.c server/flow_store.c server/flood.c
SERVER_OBJ = $(SERVER_SRC:.c=.o)

# File sorgenti Client
CLIENT_SRC = client/client_main.c
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)

all: $(SERVER_BIN) $(CLIENT_BIN)

# Compilare il Server
$(SERVER_BIN): $(SERVER_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compilare il Client
$(CLIENT_BIN): $(CLIENT_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compilare i file .c in file .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Pulire la directory dai file compilati
clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
	rm -f common/*.o server/*.o client/*.o

# Evita conflitti 
.PHONY: all clean