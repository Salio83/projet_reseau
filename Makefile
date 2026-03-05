CC := gcc
CFLAGS := -Wall -Wextra -O2
LDFLAGS :=

SRCS_CLIENT := client.c
SRCS_SERVER := server.c
BIN_CLIENT := client
BIN_SERVER := server

.PHONY: all client server chess clean run-chess

all: client server

client: $(SRCS_CLIENT)
	$(CC) $(CFLAGS) -o $(BIN_CLIENT) $(SRCS_CLIENT) $(LDFLAGS)

server: $(SRCS_SERVER)
	$(CC) $(CFLAGS) -o $(BIN_SERVER) $(SRCS_SERVER) $(LDFLAGS)

chess:
	@if [ -f shareds/raylib-chess/CMakeLists.txt ]; then \
		mkdir -p shareds/raylib-chess/build && cd shareds/raylib-chess/build && cmake .. && $(MAKE); \
	else \
		echo "shareds/raylib-chess CMakeLists.txt not found."; \
	fi

run-chess: chess
	./shareds/raylib-chess/build/RayLib_Game

clean:
	rm -f $(BIN_CLIENT) $(BIN_SERVER)
	@if [ -d shareds/raylib-chess/build ]; then cd shareds/raylib-chess/build && $(MAKE) clean || true; fi
