# Configuration du compilateur
CC = gcc
CFLAGS = -Wall -Wextra -Icommon/chess_engine -Icommon/render -Icommon/ipc_utils -Icommon/network_models -Iservices/gateway -Ilibs -DPROJECT_DIR=\"$(shell pwd)\"

# Bibliothèques
# Use locally built raylib with Wayland backend (bypasses broken X11/GLX on this machine)
RAYLIB_LIBS = libs/libraylib.a -lEGL -lwayland-client -lwayland-egl -lxkbcommon -lm -lpthread -ldl -lrt
SERVER_LIBS = -lpthread

# Chemins des sources
CHESS_SRCS = apps/chess_standalone/main.c common/chess_engine/chess.c common/render/render.c
SERVER_SRCS = services/server_main.c services/gateway/gateway.c common/ipc_utils/ipc_utils.c
AUTH_SRCS = services/auth/auth_main.c common/ipc_utils/ipc_utils.c
MATCHMAKER_SRCS = services/matchmaker/matchmaker_main.c common/ipc_utils/ipc_utils.c
GAMEWORKER_SRCS = services/game_worker/game_worker_main.c common/ipc_utils/ipc_utils.c common/chess_engine/chess.c

# Cibles principales
all: server auth_service matchmaker_service gameworker_service chess chess_gui_client test_client test_suite test_game_flow client_interactive

# Compilation du Serveur Unifié
server: $(SERVER_SRCS)
	@echo "Compilation du Serveur Unifié..."
	$(CC) $(CFLAGS) $(SERVER_SRCS) $(SERVER_LIBS) -o server_app

# Compilation du Service d'Authentification
auth_service: $(AUTH_SRCS)
	@echo "Compilation du Service d'Authentification..."
	$(CC) $(CFLAGS) $(AUTH_SRCS) -o auth_app

# Compilation du Service Matchmaker
matchmaker_service: $(MATCHMAKER_SRCS)
	@echo "Compilation du Service Matchmaker..."
	$(CC) $(CFLAGS) $(MATCHMAKER_SRCS) -o matchmaker_app

# Compilation du Service GameWorker
gameworker_service: $(GAMEWORKER_SRCS)
	@echo "Compilation du Service GameWorker..."
	$(CC) $(CFLAGS) $(GAMEWORKER_SRCS) -o gameworker_app

# Compilation du jeu d'échecs (Standalone)
chess: $(CHESS_SRCS)
	@echo "Compilation du jeu d'échecs..."
	$(CC) $(CFLAGS) $(CHESS_SRCS) $(RAYLIB_LIBS) -o chess_game

# Compilation du client GUI réseau
GUI_CLIENT_SRCS = apps/chess_gui_client/main.c common/chess_engine/chess.c common/render/render.c
chess_gui_client: $(GUI_CLIENT_SRCS)
	@echo "Compilation du client GUI réseau..."
	$(CC) $(CFLAGS) $(GUI_CLIENT_SRCS) $(RAYLIB_LIBS) -lpthread -o chess_gui_client

# Client interactif pour les tests manuels
client_interactive: tests/interactive_client.c
	@echo "Compilation du client interactif..."
	$(CC) $(CFLAGS) tests/interactive_client.c -lpthread -o client_interactive

# Autres outils de test
test_client: tests/simulate_packet.c
	$(CC) $(CFLAGS) tests/simulate_packet.c -o test_client

test_suite: tests/test_all_features.c common/ipc_utils/ipc_utils.c
	$(CC) $(CFLAGS) tests/test_all_features.c common/ipc_utils/ipc_utils.c -o test_suite

test_game_flow: tests/test_game_flow.c common/ipc_utils/ipc_utils.c
	$(CC) $(CFLAGS) tests/test_game_flow.c common/ipc_utils/ipc_utils.c -o test_game_flow

# Exécution
test: test_suite
	./test_suite

test-full: all
	bash tests/run_full_test.sh

# Nettoyage
clean:
	rm -f chess_game chess_gui_client server_app auth_app matchmaker_app gameworker_app test_client test_suite test_game_flow client_interactive gateway
	rm -rf build/

# Nettoyage manuel des ressources IPC (en cas de plantage)
clean-ipc:
	@echo "Nettoyage des files de messages..."
	ipcs -q | grep $(shell whoami) | awk '{print $$2}' | xargs -r -n1 ipcrm -q
	@echo "Nettoyage terminé."

.PHONY: all chess server gateway clean test test-full clean-ipc
