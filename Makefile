CC = gcc
CFLAGS = -Wall -Wextra -O2

SERVER_BIN = server/udp_server_linux
CLIENT_BIN = client/udp_client_linux

SERVER_SRC = server/udp_server_linux.c
CLIENT_SRC = client/udp_client_linux.c


all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $(SERVER_BIN) $(SERVER_SRC)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC)


clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)


echo:
	@echo "Compiling UDP Server and Client..."
