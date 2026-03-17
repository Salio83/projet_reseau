# Configuration du compilateur
CC = gcc
CFLAGS = -Wall -Wextra -Icommon/chess_engine -Icommon/render -Icommon/ipc_utils -Icommon/network_models -DPROJECT_DIR=\"$(shell pwd)\"
LIBS = -lpthread -lrt

# Bibliothèques pour Raylib (GUI)
RAYLIB_LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# Chemins des sources
CHESS_SRCS = apps/chess_standalone/main.c common/chess_engine/chess.c common/render/render.c
IPC_SRCS = common/ipc_utils/ipc_utils.c

# Cibles principales
all: services

# Services individuellement
gateway: services/gateway/main.c $(IPC_SRCS)
	$(CC) $(CFLAGS) $^ -o build/gateway $(LIBS)

auth: services/auth/main.c $(IPC_SRCS)
	$(CC) $(CFLAGS) $^ -o build/auth $(LIBS)

matchmaker: services/matchmaker/main.c $(IPC_SRCS)
	$(CC) $(CFLAGS) $^ -o build/matchmaker $(LIBS)

chat: services/chat/main.c $(IPC_SRCS)
	$(CC) $(CFLAGS) $^ -o build/chat $(LIBS)

db_gateway: services/db_gateway/main.c $(IPC_SRCS)
	$(CC) $(CFLAGS) $^ -o build/db_gateway $(LIBS)

game_worker: services/game_worker/main.c common/chess_engine/chess.c $(IPC_SRCS)
	$(CC) $(CFLAGS) $^ -o build/game_worker $(LIBS)

tournament: services/tournament/main.c $(IPC_SRCS)
	$(CC) $(CFLAGS) $^ -o build/tournament $(LIBS)

# Compilation de tous les services
services: create_build_dir gateway auth matchmaker chat db_gateway game_worker tournament
	@echo "Tous les services sont compilés dans build/"

create_build_dir:
	mkdir -p build

# Compilation et lancement du jeu d'échecs standalone
chess: $(CHESS_SRCS)
	@echo "Compilation du mode Chess Standalone..."
	$(CC) $(CFLAGS) $(CHESS_SRCS) $(RAYLIB_LIBS) -o chess_game
	@echo "Lancement du jeu..."
	./chess_game

# Nettoyage
clean:
	rm -f chess_game
	rm -rf build/

.PHONY: all chess services clean create_build_dir gateway auth matchmaker chat db_gateway game_worker
