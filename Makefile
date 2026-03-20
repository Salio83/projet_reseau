# Configuration du compilateur
CC = gcc
CFLAGS = -Wall -Wextra -Icommon/chess_engine -Icommon/render -Icommon/ipc_utils -Icommon/network_models -Iservices/gateway -DPROJECT_DIR=\"$(shell pwd)\"

# Bibliothèques
RAYLIB_LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
SERVER_LIBS = -lpthread

# Chemins des sources
CHESS_SRCS = apps/chess_standalone/main.c common/chess_engine/chess.c common/render/render.c
SERVER_SRCS = services/server_main.c services/gateway/gateway.c common/ipc_utils/ipc_utils.c
AUTH_SRCS = services/auth/auth_main.c common/ipc_utils/ipc_utils.c

# Cibles principales
all: server auth_service chess test_client

# Compilation du Serveur Unifié
server: $(SERVER_SRCS)
	@echo "Compilation du Serveur Unifié..."
	$(CC) $(CFLAGS) $(SERVER_SRCS) $(SERVER_LIBS) -o server_app
	@echo "Serveur prêt : ./server_app"

# Compilation du Service d'Authentification
auth_service: $(AUTH_SRCS)
	@echo "Compilation du Service d'Authentification..."
	$(CC) $(CFLAGS) $(AUTH_SRCS) -o auth_app
	@echo "Service Auth prêt : ./auth_app"

# Client de test pour simuler l'envoi de paquets
test_client: tests/simulate_packet.c
	@echo "Compilation du client de test..."
	$(CC) $(CFLAGS) tests/simulate_packet.c -o test_client
	@echo "Test client prêt : ./test_client"

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


# Comment procéder :

#    1. Compiler le tout :
#    1     make


#    2. Lancer les composants dans 3 terminaux différents :
#        * Terminal 1 (Le Service d'Auth) :
#    1         ./auth_app
#        * Terminal 2 (Le Serveur Central) :
#    1         ./server_app
#        * Terminal 3 (Le Simulateur) :
#    1         ./test_client


#   Ce que fait le simulateur (test_client) :
#    * Il se connecte au Gateway sur le port 8080.
#    * Il construit une structure AuthRequest et l'encapsule dans un PacketHeader de type PACKET_AUTH_REQ.
#    * Il envoie d'abord l'en-tête, puis les données.
#    * Il attend une réponse du Gateway (qui aura été renvoyée par le service d'authentification).

#   Exemple de code utilisé pour l'envoi :


#     1 PacketHeader header;
#     2 header.type = PACKET_AUTH_REQ;
#     3 header.length = sizeof(PacketHeader) + sizeof(AuthRequest);
#     4
#     5 AuthRequest auth;
#     6 strncpy(auth.username, "Joueur_Test", 32);
#     7
#     8 // Envoi séquentiel sur le socket TCP
#     9 send(sock, &header, sizeof(PacketHeader), 0);
#    10 send(sock, &auth, sizeof(AuthRequest), 0);


#   Vous pouvez maintenant utiliser ce modèle pour simuler d'autres types de paquets (Matchmaking, Chat, etc.) en changeant simplement le type dans le header et la structure de
#   données envoyée.