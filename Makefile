CC := gcc
CFLAGS := -Wall -Wextra -O2
LDFLAGS :=

SRCS_CLIENT := client.c
SRCS_SERVER := server.c
BIN_CLIENT := client
BIN_SERVER := server

.PHONY: all client server chess clean

all: client server

client: $(SRCS_CLIENT)
	$(CC) $(CFLAGS) -o $(BIN_CLIENT) $(SRCS_CLIENT) $(LDFLAGS)

server: $(SRCS_SERVER)
	$(CC) $(CFLAGS) -o $(BIN_SERVER) $(SRCS_SERVER) $(LDFLAGS)

chess:
	@if [ -f raylib-chess/CMakeLists.txt ]; then \
		mkdir -p raylib-chess/build && cd raylib-chess/build && cmake .. && $(MAKE); \
	else \
		echo "raylib-chess CMakeLists.txt not found."; \
	fi

clean:
	rm -f $(BIN_CLIENT) $(BIN_SERVER)
	@if [ -d raylib-chess/build ]; then cd raylib-chess/build && $(MAKE) clean || true; fi
