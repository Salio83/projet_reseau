# Configuration du compilateur
CC = gcc
CFLAGS = -Wall -Wextra -Icommon/chess_engine -Icommon/render -Icommon/ipc_utils -Icommon/network_models -DPROJECT_DIR=\"$(shell pwd)\"

# Bibliothèques pour Raylib (GUI)
RAYLIB_LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# Chemins des sources
CHESS_SRCS = apps/chess_standalone/main.c common/chess_engine/chess.c common/render/render.c

# Cibles principales
all: services

# Compilation et lancement du jeu d'échecs standalone
chess: $(CHESS_SRCS)
	@echo "🔨 Compilation du mode Chess Standalone..."
	$(CC) $(CFLAGS) $(CHESS_SRCS) $(RAYLIB_LIBS) -o chess_game
	@echo "🚀 Lancement du jeu..."
	./chess_game

# Placeholder pour les futurs services backend
services:
	@echo "A venir : Compilation des services (gateway, auth, matchmaker...)"

# Nettoyage
clean:
	rm -f chess_game
	rm -rf build/

.PHONY: all chess services clean
