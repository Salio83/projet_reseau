#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "chess.h"
#include "ipc_utils.h"
#include "packet_types.h"
#include "raylib.h"
#include "render.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_TCP_PORT 6767
#define SERVER_UDP_PORT 6768
#define SCREEN_W 1100
#define SCREEN_H 900
#define SQUARE_SIZE 80
#define MAX_CHAT_LINES 8

typedef enum {
    STATE_MENU,
    STATE_LOBBY,
    STATE_PLAYING,
    STATE_GAME_OVER
} AppScreenState;

typedef struct {
    GameState display_state;
    uint32_t room_id;
    uint32_t session_id;
    uint8_t my_color;
    int is_spectator;
    AppScreenState screen;
    pthread_mutex_t lock;
    int sock;
    int udp_sock;
    volatile int running;
    char username[MAX_USERNAME_LEN];
    char status_msg[128];
    ActiveGamesResponse active_games;
    int is_in_queue;
    uint32_t queue_size;
    uint32_t queue_pos;
    char chat_lines[MAX_CHAT_LINES][320];
    int chat_count;
    char chat_input[MAX_CHAT_MESSAGE_LEN];
    int chat_input_len;
} ClientApp;

static PieceType fen_char_to_type(char c) {
    switch (tolower(c)) {
        case 'p': return PAWN;
        case 'n': return KNIGHT;
        case 'b': return BISHOP;
        case 'r': return ROOK;
        case 'q': return QUEEN;
        case 'k': return KING;
        default: return PIECE_NONE;
    }
}

static void parse_board(GameState *state, const char *fen) {
    memset(state->board, 0, sizeof(state->board));
    int row = 0;
    int col = 0;

    for (int i = 0; fen[i] != '\0' && row < 8; i++) {
        char c = fen[i];
        if (c == '/') {
            row++;
            col = 0;
        } else if (isdigit(c)) {
            col += c - '0';
        } else if (col < 8) {
            state->board[row][col].type = fen_char_to_type(c);
            state->board[row][col].color = isupper(c) ? PLAYER_WHITE : PLAYER_BLACK;
            col++;
        }
    }
}

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

static void add_chat_line(ClientApp *app, const char *line) {
    if (app->chat_count < MAX_CHAT_LINES) {
        strncpy(app->chat_lines[app->chat_count++], line, sizeof(app->chat_lines[0]) - 1);
        return;
    }
    for (int i = 0; i < MAX_CHAT_LINES - 1; i++) {
        strncpy(app->chat_lines[i], app->chat_lines[i + 1], sizeof(app->chat_lines[i]) - 1);
    }
    strncpy(app->chat_lines[MAX_CHAT_LINES - 1], line, sizeof(app->chat_lines[0]) - 1);
}

static void send_udp_register(ClientApp *app) {
    if (app->udp_sock < 0 || app->session_id == 0) {
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
    header->session_id = app->session_id;
    request->session_id = app->session_id;

    sendto(app->udp_sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, sizeof(addr));
}

static void handle_packet_locked(ClientApp *app, PacketHeader *header, const char *payload) {
    switch (header->type) {
        case PACKET_AUTH_OK: {
            const AuthOk *ok = (const AuthOk *)payload;
            app->session_id = ok->session_id;
            strncpy(app->username, ok->username, sizeof(app->username) - 1);
            app->screen = STATE_LOBBY;
            snprintf(app->status_msg, sizeof(app->status_msg),
                     "Authentifié. Rejoignez une partie ou observez un salon.");
            send_udp_register(app);
            break;
        }
        case PACKET_GAME_STARTED: {
            const GameStarted *started = (const GameStarted *)payload;
            app->room_id = started->room_id;
            app->my_color = started->your_color;
            app->is_spectator = 0;
            app->screen = STATE_PLAYING;
            app->chat_count = 0;
            app->is_in_queue = 0;
            app->queue_size = 0;
            app->queue_pos = 0;
            game_reset(&app->display_state);
            snprintf(app->status_msg, sizeof(app->status_msg), "Partie %u contre %s.",
                     started->room_id, started->opponent_username);
            break;
        }
        case PACKET_MATCHMAKING_STATUS: {
            const MatchmakingStatus *ms = (const MatchmakingStatus *)payload;
            app->is_in_queue = 1;
            app->queue_size = ms->queue_size;
            app->queue_pos = ms->position;
            snprintf(app->status_msg, sizeof(app->status_msg), "En file d'attente... (%u/%u)",
                     app->queue_pos, app->queue_size);
            break;
        }
        case PACKET_GAME_SNAPSHOT: {
            const GameSnapshot *snapshot = (const GameSnapshot *)payload;
            app->room_id = snapshot->room_id;
            parse_board(&app->display_state, snapshot->fen_board);
            app->display_state.current_player =
                snapshot->current_turn == 0 ? PLAYER_WHITE : PLAYER_BLACK;
            snprintf(app->status_msg, sizeof(app->status_msg), "Salon %u: %s vs %s",
                     snapshot->room_id, snapshot->white_username, snapshot->black_username);
            break;
        }
        case PACKET_GAME_UPDATE_UDP: {
            const GameUpdateUDP *update = (const GameUpdateUDP *)payload;
            app->room_id = update->room_id;
            parse_board(&app->display_state, update->fen_board);
            app->display_state.current_player =
                update->current_turn == 0 ? PLAYER_WHITE : PLAYER_BLACK;
            if (app->is_spectator) {
                snprintf(app->status_msg, sizeof(app->status_msg),
                         "Observation du salon %u. Dernier coup %s -> %s", update->room_id,
                         update->from_square, update->to_square);
            } else {
                snprintf(app->status_msg, sizeof(app->status_msg), "%s",
                         app->display_state.current_player == (PlayerColor)app->my_color
                             ? "A votre tour."
                             : "Tour adverse.");
            }
            break;
        }
        case PACKET_LIST_ACTIVE_GAMES_RESP:
            memcpy(&app->active_games, payload, sizeof(app->active_games));
            snprintf(app->status_msg, sizeof(app->status_msg), "%u salons disponibles.",
                     app->active_games.game_count);
            break;
        case PACKET_SPECTATE_JOIN_OK: {
            const SpectateStatus *status = (const SpectateStatus *)payload;
            app->room_id = status->room_id;
            app->is_spectator = 1;
            app->screen = STATE_PLAYING;
            app->chat_count = 0;
            snprintf(app->status_msg, sizeof(app->status_msg), "Observation du salon %u.",
                     status->room_id);
            break;
        }
        case PACKET_SPECTATE_LEAVE_OK:
            app->room_id = 0;
            app->is_spectator = 0;
            app->screen = STATE_LOBBY;
            memset(&app->active_games, 0, sizeof(app->active_games));
            snprintf(app->status_msg, sizeof(app->status_msg), "Retour au lobby.");
            break;
        case PACKET_CHAT_BROADCAST: {
            const ChatBroadcast *broadcast = (const ChatBroadcast *)payload;
            char line[320];
            snprintf(line, sizeof(line), "[%s] %s", broadcast->author_name, broadcast->message);
            add_chat_line(app, line);
            break;
        }
        case PACKET_MOVE_ERROR:
            snprintf(app->status_msg, sizeof(app->status_msg), "Coup invalide.");
            break;
        case PACKET_ERROR: {
            const PacketError *error = (const PacketError *)payload;
            snprintf(app->status_msg, sizeof(app->status_msg), "%s", error->message);
            break;
        }
        default:
            break;
    }
}

static void *network_thread(void *arg) {
    ClientApp *app = (ClientApp *)arg;

    while (app->running) {
        fd_set readfds;
        int max_fd = app->sock > app->udp_sock ? app->sock : app->udp_sock;

        FD_ZERO(&readfds);
        if (app->sock >= 0) {
            FD_SET(app->sock, &readfds);
        }
        if (app->udp_sock >= 0) {
            FD_SET(app->udp_sock, &readfds);
        }

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) <= 0) {
            continue;
        }

        if (app->sock >= 0 && FD_ISSET(app->sock, &readfds)) {
            PacketHeader header;
            char payload[MAX_MSG_SIZE];

            if (recv_all(app->sock, &header, sizeof(header)) == -1) {
                pthread_mutex_lock(&app->lock);
                app->running = 0;
                app->screen = STATE_MENU;
                snprintf(app->status_msg, sizeof(app->status_msg), "Déconnecté du serveur.");
                pthread_mutex_unlock(&app->lock);
                break;
            }

            size_t payload_len = header.length - sizeof(PacketHeader);
            if (payload_len > 0 && recv_all(app->sock, payload, payload_len) == -1) {
                pthread_mutex_lock(&app->lock);
                app->running = 0;
                app->screen = STATE_MENU;
                snprintf(app->status_msg, sizeof(app->status_msg), "Déconnecté du serveur.");
                pthread_mutex_unlock(&app->lock);
                break;
            }

            pthread_mutex_lock(&app->lock);
            handle_packet_locked(app, &header, payload);
            pthread_mutex_unlock(&app->lock);
        }

        if (app->udp_sock >= 0 && FD_ISSET(app->udp_sock, &readfds)) {
            char buffer[MAX_MSG_SIZE];
            ssize_t received = recvfrom(app->udp_sock, buffer, sizeof(buffer), 0, NULL, NULL);
            if (received > (ssize_t)sizeof(PacketHeader)) {
                PacketHeader *header = (PacketHeader *)buffer;
                pthread_mutex_lock(&app->lock);
                handle_packet_locked(app, header, buffer + sizeof(PacketHeader));
                pthread_mutex_unlock(&app->lock);
            }
        }
    }

    return NULL;
}

static int connect_to_server(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;

    if (sock < 0) {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    return sock;
}

static int init_udp_socket(void) {
    return socket(AF_INET, SOCK_DGRAM, 0);
}

static void send_auth(ClientApp *app) {
    PacketHeader header = {PACKET_AUTH_REQ, PACKET_SIZE(AuthRequest), 0};
    AuthRequest request;

    memset(&request, 0, sizeof(request));
    strncpy(request.username, app->username, sizeof(request.username) - 1);
    strncpy(request.password_hash, "hash", sizeof(request.password_hash) - 1);

    send(app->sock, &header, sizeof(header), 0);
    send(app->sock, &request, sizeof(request), 0);
}

static void send_join(ClientApp *app) {
    PacketHeader header = {PACKET_MATCHMAKING_REQ, PACKET_SIZE(MatchmakingRequest), app->session_id};
    MatchmakingRequest request;

    memset(&request, 0, sizeof(request));
    request.player_id = app->session_id;
    strncpy(request.username, app->username, sizeof(request.username) - 1);

    send(app->sock, &header, sizeof(header), 0);
    send(app->sock, &request, sizeof(request), 0);
}

static void send_list(ClientApp *app) {
    PacketHeader header = {PACKET_LIST_ACTIVE_GAMES_REQ, sizeof(PacketHeader), app->session_id};
    send(app->sock, &header, sizeof(header), 0);
}

static void send_watch(ClientApp *app, uint32_t room_id) {
    PacketHeader header = {PACKET_SPECTATE_JOIN_REQ, PACKET_SIZE(SpectateJoinRequest), app->session_id};
    SpectateJoinRequest request;

    memset(&request, 0, sizeof(request));
    request.room_id = room_id;
    strncpy(request.username, app->username, sizeof(request.username) - 1);

    send(app->sock, &header, sizeof(header), 0);
    send(app->sock, &request, sizeof(request), 0);
}

static void send_leave(ClientApp *app) {
    PacketHeader header = {PACKET_SPECTATE_LEAVE_REQ, PACKET_SIZE(SpectateLeaveRequest), app->session_id};
    SpectateLeaveRequest request;
    request.room_id = app->room_id;
    send(app->sock, &header, sizeof(header), 0);
    send(app->sock, &request, sizeof(request), 0);
}

static void send_move(ClientApp *app, int from_row, int from_col, int to_row, int to_col) {
    PacketHeader header = {PACKET_PLAYER_MOVE, PACKET_SIZE(PlayerMove), app->session_id};
    PlayerMove move;

    memset(&move, 0, sizeof(move));
    move.game_id = app->room_id;
    move.from_square[0] = 'a' + from_col;
    move.from_square[1] = '0' + (8 - from_row);
    move.to_square[0] = 'a' + to_col;
    move.to_square[1] = '0' + (8 - to_row);

    send(app->sock, &header, sizeof(header), 0);
    send(app->sock, &move, sizeof(move), 0);
}

static void send_chat(ClientApp *app) {
    PacketHeader header = {PACKET_CHAT_MSG, PACKET_SIZE(ChatMessage), app->session_id};
    ChatMessage chat;

    if (app->chat_input_len <= 0) {
        return;
    }

    memset(&chat, 0, sizeof(chat));
    chat.room_id = app->room_id;
    strncpy(chat.message, app->chat_input, sizeof(chat.message) - 1);

    send(app->sock, &header, sizeof(header), 0);
    send(app->sock, &chat, sizeof(chat), 0);

    app->chat_input_len = 0;
    app->chat_input[0] = '\0';
}

static void render_connect_screen(ClientApp *app, char *input_buf, int *input_len) {
    ClearBackground((Color){20, 20, 40, 255});

    DrawText("CHESS EN RESEAU", SCREEN_W / 2 - 210, 150, 54, GOLD);
    DrawText("Nom d'utilisateur", SCREEN_W / 2 - 170, 300, 28, LIGHTGRAY);

    Rectangle input_rect = {SCREEN_W / 2 - 200, 350, 400, 52};
    DrawRectangleRec(input_rect, (Color){35, 35, 60, 255});
    DrawRectangleLinesEx(input_rect, 2, GRAY);
    DrawText(input_buf, (int)input_rect.x + 12, (int)input_rect.y + 12, 28, WHITE);

    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 126 && *input_len < MAX_USERNAME_LEN - 1) {
            input_buf[(*input_len)++] = (char)key;
            input_buf[*input_len] = '\0';
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && *input_len > 0) {
        input_buf[--(*input_len)] = '\0';
    }

    Rectangle button = {SCREEN_W / 2 - 110, 440, 220, 58};
    Vector2 mouse = GetMousePosition();
    int hover = CheckCollisionPointRec(mouse, button);
    DrawRectangleRec(button, hover ? DARKBLUE : (Color){28, 28, 56, 255});
    DrawRectangleLinesEx(button, 2, hover ? GOLD : WHITE);
    DrawText("CONNECTER", (int)button.x + 28, (int)button.y + 16, 28, WHITE);

    DrawText(app->status_msg, SCREEN_W / 2 - 250, SCREEN_H - 70, 20, ORANGE);

    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && *input_len > 0) {
        strncpy(app->username, input_buf, sizeof(app->username) - 1);
        app->sock = connect_to_server(SERVER_IP, SERVER_TCP_PORT);
        app->udp_sock = init_udp_socket();
        if (app->sock < 0 || app->udp_sock < 0) {
            snprintf(app->status_msg, sizeof(app->status_msg), "Connexion impossible.");
            return;
        }
        app->running = 1;
        pthread_t tid;
        pthread_create(&tid, NULL, network_thread, app);
        pthread_detach(tid);
        send_auth(app);
        snprintf(app->status_msg, sizeof(app->status_msg), "Connexion en cours...");
    }
}

static void render_lobby_screen(ClientApp *app) {
    ActiveGamesResponse games;
    char status_msg[128];
    int in_queue;
    uint32_t q_size, q_pos;

    pthread_mutex_lock(&app->lock);
    games = app->active_games;
    strncpy(status_msg, app->status_msg, sizeof(status_msg) - 1);
    in_queue = app->is_in_queue;
    q_size = app->queue_size;
    q_pos = app->queue_pos;
    pthread_mutex_unlock(&app->lock);

    ClearBackground((Color){20, 28, 24, 255});
    DrawText("LOBBY", SCREEN_W / 2 - 70, 60, 54, GREEN);
    DrawText(TextFormat("Connecte en tant que %s", app->username), 60, 130, 24, LIGHTGRAY);

    Vector2 mouse = GetMousePosition();

    if (in_queue) {
        DrawRectangle(60, 170, 500, 80, (Color){40, 40, 60, 255});
        DrawRectangleLines(60, 170, 500, 80, SKYBLUE);
        DrawText("RECHERCHE DE PARTIE EN COURS...", 80, 185, 20, SKYBLUE);
        DrawText(TextFormat("Position: %u | Total dans la file: %u", q_pos, q_size), 80, 215, 22, WHITE);
    } else {
        Rectangle join_btn = {60, 180, 260, 56};
        Rectangle list_btn = {340, 180, 220, 56};

        DrawRectangleRec(join_btn, CheckCollisionPointRec(mouse, join_btn) ? DARKGREEN : (Color){25, 70, 35, 255});
        DrawRectangleRec(list_btn, CheckCollisionPointRec(mouse, list_btn) ? DARKBLUE : (Color){30, 45, 80, 255});
        DrawText("REJOINDRE UNE PARTIE", 75, 197, 24, WHITE);
        DrawText("RAFRAICHIR", 382, 197, 24, WHITE);

        if (CheckCollisionPointRec(mouse, join_btn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            send_join(app);
        }
        if (CheckCollisionPointRec(mouse, list_btn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            send_list(app);
        }
    }

    DrawText("Salons actifs", 60, 270, 28, GOLD);

    for (uint16_t i = 0; i < games.game_count && i < 6; i++) {
        int y = 320 + (int)i * 72;
        Rectangle row = {60, (float)y, 640, 56};
        Rectangle watch_btn = {590, (float)y + 10, 100, 36};
        DrawRectangleRec(row, (Color){34, 34, 42, 255});
        DrawRectangleLinesEx(row, 1, GRAY);
        DrawText(TextFormat("Salon %u", games.games[i].room_id), 75, y + 16, 22, WHITE);
        DrawText(TextFormat("%s vs %s", games.games[i].white_username, games.games[i].black_username),
                 200, y + 16, 22, LIGHTGRAY);
        DrawText(TextFormat("Spec: %u", games.games[i].spectator_count), 470, y + 16, 20, ORANGE);

        DrawRectangleRec(watch_btn, CheckCollisionPointRec(mouse, watch_btn) ? MAROON : BROWN);
        DrawText("OBSERVER", 602, y + 18, 18, WHITE);

        if (CheckCollisionPointRec(mouse, watch_btn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            send_watch(app, games.games[i].room_id);
        }
    }

    DrawText(status_msg, 60, SCREEN_H - 50, 20, YELLOW);
}

static void handle_chat_input(ClientApp *app) {
    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key <= 126 && app->chat_input_len < MAX_CHAT_MESSAGE_LEN - 1) {
            app->chat_input[app->chat_input_len++] = (char)key;
            app->chat_input[app->chat_input_len] = '\0';
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && app->chat_input_len > 0) {
        app->chat_input[--app->chat_input_len] = '\0';
    }
    if (IsKeyPressed(KEY_ENTER)) {
        send_chat(app);
    }
}

static void render_playing_screen(ClientApp *app, PieceTextures *textures) {
    GameState local_state;
    char status_msg[128];
    char chat_lines[MAX_CHAT_LINES][320];
    int chat_count;
    int is_spectator;
    uint8_t my_color;
    uint32_t room_id;

    pthread_mutex_lock(&app->lock);
    local_state = app->display_state;
    strncpy(status_msg, app->status_msg, sizeof(status_msg) - 1);
    memcpy(chat_lines, app->chat_lines, sizeof(chat_lines));
    chat_count = app->chat_count;
    is_spectator = app->is_spectator;
    my_color = app->my_color;
    room_id = app->room_id;
    handle_chat_input(app);
    pthread_mutex_unlock(&app->lock);

    ClearBackground((Color){46, 44, 42, 255});

    int board_x = 40;
    int board_y = 80;
    int flipped = (!is_spectator && my_color == 1);
    render_board(&local_state, board_x, board_y, SQUARE_SIZE, flipped, textures);

    DrawText(TextFormat("Salon %u", room_id), 40, 20, 26, GOLD);
    DrawText(is_spectator ? "Mode spectateur" : TextFormat("Vous jouez %s", my_color == 0 ? "blanc" : "noir"),
             180, 20, 24, LIGHTGRAY);
    DrawText(status_msg, 40, SCREEN_H - 50, 18, ORANGE);

    Rectangle chat_panel = {720, 80, 330, 520};
    DrawRectangleRec(chat_panel, (Color){30, 30, 35, 255});
    DrawRectangleLinesEx(chat_panel, 1, GRAY);
    DrawText("Chat", 740, 95, 28, SKYBLUE);

    for (int i = 0; i < chat_count; i++) {
        DrawText(chat_lines[i], 740, 140 + i * 46, 18, WHITE);
    }

    Rectangle input_rect = {740, 630, 250, 44};
    Rectangle send_btn = {1000, 630, 40, 44};
    DrawRectangleRec(input_rect, (Color){42, 42, 50, 255});
    DrawRectangleLinesEx(input_rect, 1, LIGHTGRAY);
    DrawText(app->chat_input, 748, 642, 18, WHITE);
    DrawRectangleRec(send_btn, DARKBLUE);
    DrawText("OK", 1009, 642, 18, WHITE);

    if (CheckCollisionPointRec(GetMousePosition(), send_btn) &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        pthread_mutex_lock(&app->lock);
        send_chat(app);
        pthread_mutex_unlock(&app->lock);
    }

    if (is_spectator) {
        Rectangle leave_btn = {740, 690, 200, 48};
        DrawRectangleRec(leave_btn, MAROON);
        DrawText("QUITTER L'OBSERVATION", 752, 706, 18, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), leave_btn) &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            send_leave(app);
        }
    }

    if (is_spectator) {
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse = GetMousePosition();
        int screen_col = ((int)mouse.x - board_x) / SQUARE_SIZE;
        int screen_row = ((int)mouse.y - board_y) / SQUARE_SIZE;
        int col = flipped ? (7 - screen_col) : screen_col;
        int row = flipped ? (7 - screen_row) : screen_row;

        if (row >= 0 && row < 8 && col >= 0 && col < 8) {
            pthread_mutex_lock(&app->lock);
            if (app->display_state.selected_row == -1) {
                Piece piece = app->display_state.board[row][col];
                if (piece.type != PIECE_NONE &&
                    piece.color == (PlayerColor)app->my_color &&
                    app->display_state.current_player == (PlayerColor)app->my_color) {
                    app->display_state.selected_row = row;
                    app->display_state.selected_col = col;
                }
            } else {
                int from_row = app->display_state.selected_row;
                int from_col = app->display_state.selected_col;
                app->display_state.selected_row = -1;
                app->display_state.selected_col = -1;
                send_move(app, from_row, from_col, row, col);
            }
            pthread_mutex_unlock(&app->lock);
        }
    }
}

static void render_network_game_over_screen(ClientApp *app) {
    ClearBackground((Color){50, 50, 50, 255});
    DrawText("PARTIE TERMINEE", SCREEN_W / 2 - 220, 200, 56, RED);
    DrawText(app->status_msg, SCREEN_W / 2 - 250, 300, 24, GOLD);
}

int main(void) {
    ClientApp app;
    memset(&app, 0, sizeof(app));
    pthread_mutex_init(&app.lock, NULL);
    app.sock = -1;
    app.udp_sock = -1;
    app.screen = STATE_MENU;
    snprintf(app.status_msg, sizeof(app.status_msg), "Entrez un pseudo.");

    InitWindow(SCREEN_W, SCREEN_H, "Chess - Client Reseau");
    SetTargetFPS(60);

    PieceTextures textures = {0};
    load_piece_textures(&textures);

    char username_input[MAX_USERNAME_LEN] = {0};
    int username_len = 0;

    while (!WindowShouldClose()) {
        BeginDrawing();

        pthread_mutex_lock(&app.lock);
        AppScreenState screen = app.screen;
        pthread_mutex_unlock(&app.lock);

        switch (screen) {
            case STATE_MENU:
                render_connect_screen(&app, username_input, &username_len);
                break;
            case STATE_LOBBY:
                render_lobby_screen(&app);
                break;
            case STATE_PLAYING:
                render_playing_screen(&app, &textures);
                break;
            case STATE_GAME_OVER:
                render_network_game_over_screen(&app);
                break;
        }

        EndDrawing();
    }

    app.running = 0;
    if (app.sock >= 0) {
        close(app.sock);
    }
    if (app.udp_sock >= 0) {
        close(app.udp_sock);
    }
    unload_piece_textures(&textures);
    pthread_mutex_destroy(&app.lock);
    CloseWindow();
    return 0;
}
