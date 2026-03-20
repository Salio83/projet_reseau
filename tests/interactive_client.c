#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include "../common/network_models/packet_types.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

int sock = 0;
int running = 1;
uint32_t current_game_id = 0;
uint8_t my_color = 0; // 0=White, 1=Black

void display_board(const char* board_str) {
    printf("\n  a b c d e f g h\n");
    int r = 8;
    printf("%d ", r--);
    for (int i = 0; board_str[i] != '\0'; i++) {
        if (board_str[i] == '/') {
            if (r > 0) printf("\n%d ", r--);
        } else {
            printf("%c ", board_str[i]);
        }
    }
    printf("\n");
}

void* receive_thread(void* arg) {
    char buffer[2048];
    while (running) {
        int valread = read(sock, buffer, 2048);
        if (valread <= 0) {
            printf("\n[Client] Déconnecté du serveur.\n");
            running = 0;
            break;
        }

        PacketHeader* header = (PacketHeader*)buffer;
        char* payload = buffer + sizeof(PacketHeader);

        switch (header->type) {
            case PACKET_AUTH_REQ: {
                AuthRequest* auth = (AuthRequest*)payload;
                printf("\n[Serveur] Authentification réussie pour : %s\n> ", auth->username);
                break;
            }
            case PACKET_GAME_STARTED: {
                GameStarted* gs = (GameStarted*)payload;
                current_game_id = gs->game_id;
                my_color = gs->your_color;
                printf("\n[Serveur] PARTIE DÉMARRÉE ! ID: %d, Adversaire: %d, Couleur: %s\n", 
                       gs->game_id, gs->opponent_id, my_color == 0 ? "BLANC (joue en premier)" : "NOIR");
                printf("> ");
                break;
            }
            case PACKET_GAME_STATE_UDP: {
                GameStateUDP* gsu = (GameStateUDP*)payload;
                printf("\n--- Mise à jour Partie %d ---", gsu->game_id);
                display_board(gsu->fen_board);
                printf("Tour : %s\n", gsu->current_turn == 0 ? "BLANC" : "NOIR");
                if (gsu->current_turn == my_color) printf("** C'EST VOTRE TOUR **\n");
                printf("> ");
                break;
            }
            case PACKET_MOVE_ERROR: {
                printf("\n[Serveur] ERREUR : Coup invalide ou ce n'est pas votre tour !\n> ");
                break;
            }
            default:
                printf("\n[Serveur] Paquet reçu (Type: %d)\n> ", header->type);
                break;
        }
        fflush(stdout);
    }
    return NULL;
}

void print_help() {
    printf("\nCommandes disponibles :\n");
    printf("  auth <username>      : S'authentifier\n");
    printf("  join                 : Rejoindre la file d'attente (matchmaking)\n");
    printf("  move <from> <to>     : Envoyer un coup (ex: move e2 e4)\n");
    printf("  help                 : Afficher cette aide\n");
    printf("  quit                 : Quitter\n");
}

int main() {
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { perror("Socket error"); return -1; }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed"); return -1;
    }

    printf("=== CLIENT INTERACTIF CHESS ===\n");
    print_help();

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, receive_thread, NULL);

    char line[256];
    while (running) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;

        char* cmd = strtok(line, " ");
        if (!cmd) continue;

        if (strcmp(cmd, "quit") == 0) {
            running = 0; break;
        } else if (strcmp(cmd, "auth") == 0) {
            char* user = strtok(NULL, " ");
            if (!user) { printf("Usage: auth <username>\n"); continue; }
            PacketHeader h = {PACKET_AUTH_REQ, sizeof(PacketHeader) + sizeof(AuthRequest), 0};
            AuthRequest req; strncpy(req.username, user, 32); strncpy(req.password_hash, "hash", 64);
            send(sock, &h, sizeof(h), 0); send(sock, &req, sizeof(req), 0);
        } else if (strcmp(cmd, "join") == 0) {
            PacketHeader h = {PACKET_MATCHMAKING_REQ, sizeof(PacketHeader), 0};
            send(sock, &h, sizeof(h), 0);
        } else if (strcmp(cmd, "move") == 0) {
            char* from = strtok(NULL, " "); char* to = strtok(NULL, " ");
            if (!from || !to) { printf("Usage: move <from> <to>\n"); continue; }
            PacketHeader h = {PACKET_PLAYER_MOVE, sizeof(PacketHeader) + sizeof(PlayerMove), 0};
            PlayerMove move; move.game_id = current_game_id;
            strncpy(move.from_square, from, 3); strncpy(move.to_square, to, 3);
            send(sock, &h, sizeof(h), 0); send(sock, &move, sizeof(move), 0);
        }
    }
    close(sock);
    return 0;
}
