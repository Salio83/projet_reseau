# Configuration du compilateur
CC = gcc
CFLAGS = -Wall -Wextra -Icommon/chess_engine -Icommon/render -Icommon/ipc_utils -Icommon/network_models -Iservices/gateway -Ilibs -DPROJECT_DIR='"$(shell pwd)"'

# Bibliothèques
# Use locally downloaded raylib since apt package is missing
RAYLIB_LIBS = libs/libraylib.a -lGL -lm -lpthread -ldl -lrt -lX11
SERVER_LIBS = -lpthread

# Chemins des sources
CHESS_SRCS = apps/chess_standalone/main.c common/chess_engine/chess.c common/render/render.c
SERVER_SRCS = services/server_main.c services/gateway/gateway.c common/ipc_utils/ipc_utils.c
AUTH_SRCS = services/auth/auth_main.c common/ipc_utils/ipc_utils.c
MATCHMAKER_SRCS = services/matchmaker/matchmaker_main.c common/ipc_utils/ipc_utils.c
GAMEWORKER_SRCS = services/game_worker/game_worker_main.c common/ipc_utils/ipc_utils.c common/chess_engine/chess.c
CHAT_SRCS = services/chat/chat_main.c common/ipc_utils/ipc_utils.c
TOURNAMENT_SRCS = services/tournament/tournament_main.c common/ipc_utils/ipc_utils.c
STORAGE_SRCS = services/storage_worker/storage_worker_main.c common/ipc_utils/ipc_utils.c

# Cibles principales
all: server auth_service matchmaker_service gameworker_service chat_service tournament_service storage_service chess chess_gui_client client_interactive

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

# Compilation du Service Chat
chat_service: $(CHAT_SRCS)
	@echo "Compilation du Service Chat..."
	$(CC) $(CFLAGS) $(CHAT_SRCS) -o chat_app

# Compilation du Service Tournament
tournament_service: $(TOURNAMENT_SRCS)
	@echo "Compilation du Service Tournament..."
	$(CC) $(CFLAGS) $(TOURNAMENT_SRCS) -o tournament_app

# Compilation du Service Storage (Historique)
storage_service: $(STORAGE_SRCS)
	@echo "Compilation du Service Storage..."
	$(CC) $(CFLAGS) $(STORAGE_SRCS) -o storage_app

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

# Lancement des services
run: all
	@echo "=== Lancement des Services Chess ==="
	@./auth_app > /dev/null 2>&1 & echo $$! > .auth.pid
	@./matchmaker_app > /dev/null 2>&1 & echo $$! > .matchmaker.pid
	@./gameworker_app > /dev/null 2>&1 & echo $$! > .gameworker.pid
	@./chat_app > /dev/null 2>&1 & echo $$! > .chat.pid
	@./tournament_app > /dev/null 2>&1 & echo $$! > .tournament.pid
	@./storage_app > /dev/null 2>&1 & echo $$! > .storage.pid
	@./server_app > /dev/null 2>&1 & echo $$! > .server.pid
	@echo "Tous les services sont lancés."
	@echo "Appuyez sur Ctrl+C pour arrêter les services."
	@trap 'kill $$(cat .auth.pid .matchmaker.pid .gameworker.pid .chat.pid .tournament.pid .storage.pid .server.pid) 2>/dev/null; rm -f .*.pid; $(MAKE) clean-ipc; echo "\nServices arrêtés."; exit 0' SIGINT SIGTERM; \
	while true; do sleep 1; done

# Nettoyage
clean:
	rm -f chess_game chess_gui_client server_app auth_app matchmaker_app gameworker_app chat_app tournament_app storage_app client_interactive gateway .*.pid
	rm -rf build/
# Nettoyage manuel des ressources IPC (en cas de plantage)
clean-ipc:
	@echo "Nettoyage des files de messages..."
	ipcs -q | grep $(shell whoami) | awk '{print $$2}' | xargs -r -n1 ipcrm -q
	@echo "Nettoyage terminé."

.PHONY: all chess server gateway clean clean-ipc run
