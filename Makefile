# Configuration du compilateur
CC = gcc
CFLAGS = -Wall -Wextra -Icommon/chess_engine -Icommon/render -Icommon/ipc_utils -Icommon/network_models -Iservices/gateway -DPROJECT_DIR=\"$(shell pwd)\"

# Bibliothèques
RAYLIB_LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
SERVER_LIBS = -lpthread

# Chemins des sources
CHESS_SRCS = apps/chess_standalone/main.c common/chess_engine/chess.c common/render/render.c
SERVER_SRCS = services/server_main.c services/gateway/gateway.c common/ipc_utils/ipc_utils.c

# Cibles principales
all: server chess

# Compilation du Serveur Unifié
server: $(SERVER_SRCS)
	@echo "Compilation du Serveur Unifié..."
	$(CC) $(CFLAGS) $(SERVER_SRCS) $(SERVER_LIBS) -o server_app
	@echo "Serveur prêt : ./server_app"

# Lancement du Gateway (alias vers le serveur unifié pour compatibilité)
gateway: server
	./server_app

# Compilation et lancement du jeu d'échecs standalone
chess: $(CHESS_SRCS)
	@echo "Compilation du mode Chess Standalone..."
	$(CC) $(CFLAGS) $(CHESS_SRCS) $(RAYLIB_LIBS) -o chess_game
	@echo "Lancement du jeu..."
	./chess_game

# Nettoyage
clean:
	rm -f chess_game server_app gateway
	rm -rf build/

.PHONY: all chess server gateway clean
