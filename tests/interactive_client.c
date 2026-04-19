#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common/ipc_utils/ipc_utils.h"
#include "../common/network_models/packet_types.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_TCP_PORT 6767
#define SERVER_UDP_PORT 6768

static int tcp_sock = -1;
static int udp_sock = -1;
static int running = 1;
static uint32_t session_id = 0;
static uint32_t current_room_id = 0;
static uint8_t my_color = 0;
static char current_username[MAX_USERNAME_LEN];

static int recv_all(int fd, void *buffer, size_t len) {
    char *cursor = (char *)buffer;
    size_t received = 0;
    while (received < len) {
        ssize_t chunk = recv(fd, cursor + received, len - received, 0);
        if (chunk <= 0) {
            return -1;
        }
        received += (size_t)chunk;
    }
    return 0;
}

static void display_board(const char *board_str) {
    printf("\n  a b c d e f g h\n");
    int rank = 8;
    printf("%d ", rank--);
    for (int i = 0; board_str[i] != '\0'; i++) {
        if (board_str[i] == '/') {
            if (rank > 0) {
                printf("\n%d ", rank--);
            }
        } else {
            printf("%c ", board_str[i]);
        }
    }
    printf("\n");
}

static void send_udp_register(void) {
    if (udp_sock < 0 || session_id == 0) {
        return;
    }

    char buffer[sizeof(PacketHeader) + sizeof(UdpRegisterRequest)];
    PacketHeader *header = (PacketHeader *)buffer;
    UdpRegisterRequest *request = (UdpRegisterRequest *)(buffer + sizeof(PacketHeader));
    struct sockaddr_in addr;

    memset(buffer, 0, sizeof(buffer));
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_UDP_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    header->type = PACKET_UDP_REGISTER_REQ;
    header->length = sizeof(buffer);
    header->session_id = session_id;
    request->session_id = session_id;

    sendto(udp_sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, sizeof(addr));
}

static void handle_packet(PacketHeader *header, const char *payload) {
    switch (header->type) {
        case PACKET_AUTH_OK: {
            const AuthOk *ok = (const AuthOk *)payload;
            session_id = ok->session_id;
            strncpy(current_username, ok->username, sizeof(current_username) - 1);
            printf("\n[Serveur] Auth OK pour %s (session %u)\n> ", ok->username, ok->session_id);
            send_udp_register();
            break;
        }
        case PACKET_GAME_STARTED: {
            const GameStarted *started = (const GameStarted *)payload;
            current_room_id = started->room_id;
            my_color = started->your_color;
            printf("\n[Serveur] Partie %u démarrée contre %s. Couleur: %s\n> ", started->room_id,
                   started->opponent_username, my_color == 0 ? "BLANC" : "NOIR");
            break;
        }
        case PACKET_GAME_SNAPSHOT: {
            const GameSnapshot *snapshot = (const GameSnapshot *)payload;
            current_room_id = snapshot->room_id;
            printf("\n[Serveur] Snapshot salon %u (%s vs %s)\n", snapshot->room_id,
                   snapshot->white_username, snapshot->black_username);
            display_board(snapshot->fen_board);
            printf("Tour: %s\n> ", snapshot->current_turn == 0 ? "BLANC" : "NOIR");
            break;
        }
        case PACKET_GAME_UPDATE_UDP: {
            const GameUpdateUDP *update = (const GameUpdateUDP *)payload;
            current_room_id = update->room_id;
            printf("\n[Serveur] Update salon %u, coup %s -> %s\n", update->room_id,
                   update->from_square, update->to_square);
            display_board(update->fen_board);
            printf("Tour: %s\n> ", update->current_turn == 0 ? "BLANC" : "NOIR");
            break;
        }
        case PACKET_LIST_ACTIVE_GAMES_RESP: {
            const ActiveGamesResponse *response = (const ActiveGamesResponse *)payload;
            printf("\n[Serveur] Salons actifs (%u)%s\n", response->game_count,
                   response->truncated ? " - liste tronquée" : "");
            for (uint16_t i = 0; i < response->game_count; i++) {
                const ActiveGameInfo *info = &response->games[i];
                printf("  salon %u : %s vs %s, spectateurs=%u, coups=%u\n",
                       info->room_id, info->white_username, info->black_username,
                       info->spectator_count, info->move_count);
            }
            printf("> ");
            break;
        }
        case PACKET_SPECTATE_JOIN_OK: {
            const SpectateStatus *status = (const SpectateStatus *)payload;
            current_room_id = status->room_id;
            printf("\n[Serveur] Observation du salon %u activée.\n> ", status->room_id);
            break;
        }
        case PACKET_SPECTATE_LEAVE_OK:
            current_room_id = 0;
            printf("\n[Serveur] Observation arrêtée.\n> ");
            break;
        case PACKET_CHAT_BROADCAST: {
            const ChatBroadcast *broadcast = (const ChatBroadcast *)payload;
            printf("\n[Chat][%u][%s] %s\n> ", broadcast->room_id, broadcast->author_name,
                   broadcast->message);
            break;
        }
        case PACKET_MOVE_ERROR:
            printf("\n[Serveur] Coup invalide ou hors tour.\n> ");
            break;
        case PACKET_ERROR: {
            const PacketError *error = (const PacketError *)payload;
            printf("\n[Serveur] Erreur %u: %s\n> ", error->code, error->message);
            break;
        }
        case PACKET_TOURNAMENT_CREATE_RESP: {
            const TournamentCreateResp *resp = (const TournamentCreateResp *)payload;
            printf("\n[Serveur] Tournoi cree avec succes. ID: %u\n> ", resp->tournament_id);
            break;
        }
        case PACKET_TOURNAMENT_JOIN_RESP: {
            const TournamentJoinResp *resp = (const TournamentJoinResp *)payload;
            if (resp->status) {
                printf("\n[Serveur] Rejoint le tournoi avec succes.\n> ");
            } else {
                printf("\n[Serveur] Echec rejoindre le tournoi: %s\n> ", resp->message);
            }
            break;
        }
        case PACKET_TOURNAMENT_LIST_RESP: {
            const TournamentListResp *resp = (const TournamentListResp *)payload;
            printf("\n[Serveur] Tournois en attente (%u)\n", resp->tournament_count);
            for (uint16_t i = 0; i < resp->tournament_count; i++) {
                printf("  [Tournoi ID %u] Joueurs: %u/%u\n", 
                       resp->tournaments[i].tournament_id,
                       resp->tournaments[i].player_count,
                       resp->tournaments[i].max_players);
            }
            printf("> ");
            break;
        }
        case PACKET_GAME_OVER: {
            const GameOver *go = (const GameOver *)payload;
            if (go->result == 1) {
                printf("\n========================================\n");
                printf("  ECHEC ET MAT ! Les BLANCS gagnent !\n");
                printf("  Vainqueur : %s\n", go->winner_name);
                printf("========================================\n> ");
            } else if (go->result == 2) {
                printf("\n========================================\n");
                printf("  ECHEC ET MAT ! Les NOIRS gagnent !\n");
                printf("  Vainqueur : %s\n", go->winner_name);
                printf("========================================\n> ");
            } else {
                printf("\n========================================\n");
                printf("  PAT ! Match nul.\n");
                printf("========================================\n> ");
            }
            current_room_id = 0;
            break;
        }
        default:
            printf("\n[Serveur] Paquet reçu: %u\n> ", header->type);
            break;
    }
    fflush(stdout);
}

static void *receive_thread(void *unused) {
    (void)unused;

    while (running) {
        fd_set readfds;
        int max_fd = tcp_sock > udp_sock ? tcp_sock : udp_sock;

        FD_ZERO(&readfds);
        if (tcp_sock >= 0) {
            FD_SET(tcp_sock, &readfds);
        }
        if (udp_sock >= 0) {
            FD_SET(udp_sock, &readfds);
        }

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) <= 0) {
            continue;
        }

        if (tcp_sock >= 0 && FD_ISSET(tcp_sock, &readfds)) {
            PacketHeader header;
            char payload[MAX_MSG_SIZE];

            if (recv_all(tcp_sock, &header, sizeof(header)) == -1) {
                running = 0;
                break;
            }

            size_t payload_len = header.length - sizeof(PacketHeader);
            if (payload_len > 0 && recv_all(tcp_sock, payload, payload_len) == -1) {
                running = 0;
                break;
            }
            handle_packet(&header, payload);
        }

        if (udp_sock >= 0 && FD_ISSET(udp_sock, &readfds)) {
            char buffer[MAX_MSG_SIZE];
            ssize_t received = recvfrom(udp_sock, buffer, sizeof(buffer), 0, NULL, NULL);
            if (received > (ssize_t)sizeof(PacketHeader)) {
                PacketHeader *header = (PacketHeader *)buffer;
                handle_packet(header, buffer + sizeof(PacketHeader));
            }
        }
    }

    printf("\n[Client] Déconnecté.\n");
    return NULL;
}

static void print_help(void) {
    printf("\nCommandes disponibles :\n");
    printf("  auth <username>   : s'authentifier\n");
    printf("  join              : rejoindre le matchmaking\n");
    printf("  list              : lister les salons actifs\n");
    printf("  watch <room_id>   : observer un salon\n");
    printf("  leave             : quitter le mode spectateur\n");
    printf("  move <from> <to>  : jouer un coup\n");
    printf("  chat <texte>      : envoyer un message au salon courant\n");
    printf("  tourney_create <max_players>: creer un tournoi\n");
    printf("  tourney_join <id>           : rejoindre un tournoi\n");
    printf("  tourney_list                : lister les tournois en attente\n");
    printf("  help              : afficher l'aide\n");
    printf("  quit              : quitter\n");
}

int main(void) {
    struct sockaddr_in tcp_addr;
    pthread_t thread_id;

    tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (tcp_sock < 0 || udp_sock < 0) {
        perror("socket");
        return -1;
    }

    memset(&tcp_addr, 0, sizeof(tcp_addr));
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_port = htons(SERVER_TCP_PORT);
    inet_pton(AF_INET, SERVER_IP, &tcp_addr.sin_addr);

    if (connect(tcp_sock, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0) {
        perror("connect");
        return -1;
    }

    printf("=== CLIENT INTERACTIF CHESS ===\n");
    print_help();

    pthread_create(&thread_id, NULL, receive_thread, NULL);

    char line[512];
    while (running) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        line[strcspn(line, "\n")] = '\0';

        char *cmd = strtok(line, " ");
        if (!cmd) {
            continue;
        }

        if (strcmp(cmd, "quit") == 0) {
            running = 0;
        } else if (strcmp(cmd, "help") == 0) {
            print_help();
        } else if (strcmp(cmd, "auth") == 0) {
            char *user = strtok(NULL, " ");
            if (!user) {
                printf("Usage: auth <username>\n");
                continue;
            }
            PacketHeader header = {PACKET_AUTH_REQ, PACKET_SIZE(AuthRequest), 0};
            AuthRequest request;
            memset(&request, 0, sizeof(request));
            strncpy(request.username, user, sizeof(request.username) - 1);
            strncpy(request.password_hash, "hash", sizeof(request.password_hash) - 1);
            send(tcp_sock, &header, sizeof(header), 0);
            send(tcp_sock, &request, sizeof(request), 0);
        } else if (strcmp(cmd, "join") == 0) {
            PacketHeader header = {PACKET_MATCHMAKING_REQ, PACKET_SIZE(MatchmakingRequest), session_id};
            MatchmakingRequest request;
            memset(&request, 0, sizeof(request));
            request.player_id = session_id;
            strncpy(request.username, current_username, sizeof(request.username) - 1);
            send(tcp_sock, &header, sizeof(header), 0);
            send(tcp_sock, &request, sizeof(request), 0);
        } else if (strcmp(cmd, "list") == 0) {
            PacketHeader header = {PACKET_LIST_ACTIVE_GAMES_REQ, sizeof(PacketHeader), session_id};
            send(tcp_sock, &header, sizeof(header), 0);
        } else if (strcmp(cmd, "watch") == 0) {
            char *room_str = strtok(NULL, " ");
            if (!room_str) {
                printf("Usage: watch <room_id>\n");
                continue;
            }
            PacketHeader header = {PACKET_SPECTATE_JOIN_REQ, PACKET_SIZE(SpectateJoinRequest), session_id};
            SpectateJoinRequest request;
            memset(&request, 0, sizeof(request));
            request.room_id = (uint32_t)strtoul(room_str, NULL, 10);
            strncpy(request.username, current_username, sizeof(request.username) - 1);
            send(tcp_sock, &header, sizeof(header), 0);
            send(tcp_sock, &request, sizeof(request), 0);
        } else if (strcmp(cmd, "leave") == 0) {
            PacketHeader header = {PACKET_SPECTATE_LEAVE_REQ, PACKET_SIZE(SpectateLeaveRequest), session_id};
            SpectateLeaveRequest request;
            request.room_id = current_room_id;
            send(tcp_sock, &header, sizeof(header), 0);
            send(tcp_sock, &request, sizeof(request), 0);
        } else if (strcmp(cmd, "move") == 0) {
            char *from = strtok(NULL, " ");
            char *to = strtok(NULL, " ");
            if (!from || !to) {
                printf("Usage: move <from> <to>\n");
                continue;
            }
            PacketHeader header = {PACKET_PLAYER_MOVE, PACKET_SIZE(PlayerMove), session_id};
            PlayerMove move;
            memset(&move, 0, sizeof(move));
            move.game_id = current_room_id;
            strncpy(move.from_square, from, sizeof(move.from_square) - 1);
            strncpy(move.to_square, to, sizeof(move.to_square) - 1);
            send(tcp_sock, &header, sizeof(header), 0);
            send(tcp_sock, &move, sizeof(move), 0);
        } else if (strcmp(cmd, "chat") == 0) {
            char *message = strtok(NULL, "");
            if (!message) {
                printf("Usage: chat <texte>\n");
                continue;
            }
            PacketHeader header = {PACKET_CHAT_MSG, PACKET_SIZE(ChatMessage), session_id};
            ChatMessage chat;
            memset(&chat, 0, sizeof(chat));
            chat.room_id = current_room_id;
            strncpy(chat.message, message, sizeof(chat.message) - 1);
            send(tcp_sock, &header, sizeof(header), 0);
            send(tcp_sock, &chat, sizeof(chat), 0);
        } else if (strcmp(cmd, "tourney_create") == 0) {
            char *max_str = strtok(NULL, " ");
            if (!max_str) {
                printf("Usage: tourney_create <max_players>\n");
                continue;
            }
            PacketHeader header = {PACKET_TOURNAMENT_CREATE_REQ, PACKET_SIZE(TournamentCreateReq), session_id};
            TournamentCreateReq req;
            req.max_players = (uint8_t)atoi(max_str);
            send(tcp_sock, &header, sizeof(header), 0);
            send(tcp_sock, &req, sizeof(req), 0);
        } else if (strcmp(cmd, "tourney_join") == 0) {
            char *id_str = strtok(NULL, " ");
            if (!id_str) {
                printf("Usage: tourney_join <tournament_id>\n");
                continue;
            }
            PacketHeader header = {PACKET_TOURNAMENT_JOIN_REQ, PACKET_SIZE(TournamentJoinReq), session_id};
            TournamentJoinReq req;
            memset(&req, 0, sizeof(req));
            req.tournament_id = (uint32_t)atoi(id_str);
            strncpy(req.username, current_username, sizeof(req.username) - 1);
            send(tcp_sock, &header, sizeof(header), 0);
            send(tcp_sock, &req, sizeof(req), 0);
        } else if (strcmp(cmd, "tourney_list") == 0) {
            PacketHeader header = {PACKET_TOURNAMENT_LIST_REQ, sizeof(PacketHeader), session_id};
            send(tcp_sock, &header, sizeof(header), 0);
        }
    }

    close(tcp_sock);
    close(udp_sock);
    pthread_join(thread_id, NULL);
    return 0;
}
