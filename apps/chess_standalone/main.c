#include "raylib.h"
#include "chess.h"
#include "render.h"
#include "packet_types.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6767

typedef enum {
    STATE_MENU,
    STATE_MATCHMAKING,
    STATE_PLAYING,
    STATE_GAME_OVER
} GameScreenState;

typedef struct {
    int sock;
    uint32_t current_game_id;
    uint8_t my_color;
    bool connected;
    char net_buffer[4096];
    size_t net_buffer_len;
} NetworkContext;

void pos_to_algebraic(int row, int col, char* out) {
    out[0] = 'a' + col;
    out[1] = '8' - row;
    out[2] = '\0';
}

int connect_to_server() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return -1;
    }

    // Mode non-bloquant
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    return sock;
}

void send_join_matchmaking(int sock) {
    PacketHeader h = {PACKET_MATCHMAKING_REQ, sizeof(PacketHeader), 0};
    if (send(sock, &h, sizeof(h), 0) < 0) {
        perror("send matchmaking failed");
    } else {
        printf("Requête matchmaking envoyée au serveur.\n");
    }
}

void send_move(int sock, uint32_t game_id, int from_row, int from_col, int to_row, int to_col) {
    PacketHeader h = {PACKET_PLAYER_MOVE, sizeof(PacketHeader) + sizeof(PlayerMove), 0};
    PlayerMove move;
    move.game_id = game_id;
    pos_to_algebraic(from_row, from_col, move.from_square);
    pos_to_algebraic(to_row, to_col, move.to_square);
    move.promotion = '\0';
    
    char full_packet[sizeof(PacketHeader) + sizeof(PlayerMove)];
    memcpy(full_packet, &h, sizeof(PacketHeader));
    memcpy(full_packet + sizeof(PacketHeader), &move, sizeof(PlayerMove));
    
    if (send(sock, full_packet, sizeof(full_packet), 0) < 0) {
        perror("send move failed");
    }
}

void process_network(NetworkContext* net, GameState* game, GameScreenState* state) {
    if (!net->connected) return;

    char tmp_buf[1024];
    ssize_t bytes = recv(net->sock, tmp_buf, sizeof(tmp_buf), 0);
    
    if (bytes > 0) {
        if (net->net_buffer_len + bytes <= sizeof(net->net_buffer)) {
            memcpy(net->net_buffer + net->net_buffer_len, tmp_buf, bytes);
            net->net_buffer_len += bytes;
        }
    } else if (bytes == 0 || (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        printf("Connexion perdue avec le serveur.\n");
        net->connected = false;
        close(net->sock);
        *state = STATE_MENU;
        return;
    }

    // Traitement des paquets complets
    while (net->net_buffer_len >= sizeof(PacketHeader)) {
        PacketHeader* h = (PacketHeader*)net->net_buffer;
        if (net->net_buffer_len < h->length) break; // Paquet incomplet

        char* payload = net->net_buffer + sizeof(PacketHeader);
        
        switch (h->type) {
            case PACKET_GAME_STARTED: {
                GameStarted* gs = (GameStarted*)payload;
                net->current_game_id = gs->game_id;
                net->my_color = gs->your_color;
                *state = STATE_PLAYING;
                game_reset(game);
                printf("PARTIE DÉMARRÉE ! ID=%d, Vous jouez les %s\n", 
                       gs->game_id, gs->your_color == 0 ? "Blancs" : "Noirs");
                break;
            }
            case PACKET_GAME_STATE_UDP: {
                GameStateUDP* gsu = (GameStateUDP*)payload;
                if (gsu->game_id == net->current_game_id) {
                    game_from_fen(game, gsu->fen_board);
                }
                break;
            }
            case PACKET_MOVE_ERROR: {
                printf("Mouvement invalide refusé par le serveur.\n");
                break;
            }
        }
        
        size_t handled = h->length;
        if (handled < sizeof(PacketHeader)) handled = sizeof(PacketHeader);
        if (handled > net->net_buffer_len) handled = net->net_buffer_len;
        
        memmove(net->net_buffer, net->net_buffer + handled, net->net_buffer_len - handled);
        net->net_buffer_len -= handled;
    }
}

int main(void)
{
    const int screenWidth = 1000;
    const int screenHeight = 900;

    InitWindow(screenWidth, screenHeight, "World Polytech Chess - Online");
    SetTargetFPS(60);

    PieceTextures textures = {0};
    load_piece_textures(&textures);

    GameState* game = game_init();
    GameScreenState screen_state = STATE_MENU;
    int hovered_button = -1;

    int board_x = (screenWidth - 8 * 80) / 2;
    int board_y = 50;
    int square_size = 80;

    NetworkContext net = {-1, 0, 0, false, {0}, 0};

    while (!WindowShouldClose())
    {
        process_network(&net, game, &screen_state);

        BeginDrawing();
        ClearBackground((Color){30, 30, 30, 255});

        if (screen_state == STATE_MENU) {
            render_menu(screenWidth, screenHeight, &hovered_button);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (hovered_button == 1) { // Bouton "Play"
                    net.sock = connect_to_server();
                    if (net.sock >= 0) {
                        net.connected = true;
                        net.net_buffer_len = 0;
                        send_join_matchmaking(net.sock);
                        screen_state = STATE_MATCHMAKING;
                    } else {
                        printf("Erreur: Impossible de se connecter au serveur backend (Port 6767).\n");
                    }
                } else if (hovered_button == 2) {
                    break;
                }
            }
        }
        else if (screen_state == STATE_MATCHMAKING) {
            DrawText("Recherche d'un adversaire...", screenWidth/2 - 150, screenHeight/2 - 20, 20, RAYWHITE);
            DrawText("En attente de connexion sur le serveur...", screenWidth/2 - 180, screenHeight/2 + 20, 15, GRAY);
            if (IsKeyPressed(KEY_ESCAPE)) {
                screen_state = STATE_MENU;
                if (net.connected) {
                    close(net.sock);
                    net.connected = false;
                }
            }
        }
        else if (screen_state == STATE_PLAYING) {
            render_board(game, board_x, board_y, square_size, &textures);
            render_ui_info(game, screenWidth, screenHeight);

            DrawText(TextFormat("ID Partie: %d", net.current_game_id), 20, 20, 20, RAYWHITE);
            DrawText(TextFormat("Vous êtes: %s", net.my_color == 0 ? "BLANCS" : "NOIRS"), 20, 50, 20, net.my_color == 0 ? WHITE : GRAY);
            
            if (game->current_player == (PlayerColor)net.my_color) {
                DrawRectangle(15, 80, 200, 30, (Color){0, 228, 48, 100});
                DrawText("C'EST VOTRE TOUR", 20, 85, 20, GOLD);
            } else {
                DrawText("Attente de l'adversaire...", 20, 85, 20, GRAY);
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Vector2 mouse_pos = GetMousePosition();
                int col = (mouse_pos.x - board_x) / square_size;
                int row = (mouse_pos.y - board_y) / square_size;

                if (row >= 0 && row < 8 && col >= 0 && col < 8) {
                    if (game->selected_row == -1) {
                        if (game->board[row][col].type != PIECE_NONE &&
                            game->board[row][col].color == (PlayerColor)net.my_color) {
                            game->selected_row = row;
                            game->selected_col = col;
                        }
                    } else {
                        if (game->selected_row == row && game->selected_col == col) {
                            game->selected_row = -1;
                            game->selected_col = -1;
                        } else {
                            if (game->current_player == (PlayerColor)net.my_color) {
                                send_move(net.sock, net.current_game_id, game->selected_row, game->selected_col, row, col);
                            }
                            game->selected_row = -1;
                            game->selected_col = -1;
                        }
                    }
                }
            }

            if (game->checkmate || game->stalemate) {
                screen_state = STATE_GAME_OVER;
            }
        }
        else if (screen_state == STATE_GAME_OVER) {
            render_board(game, board_x, board_y, square_size, &textures);
            render_ui_info(game, screenWidth, screenHeight);
            render_game_over_screen(screenWidth, screenHeight, game, &hovered_button);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (hovered_button == 1) { 
                    send_join_matchmaking(net.sock);
                    screen_state = STATE_MATCHMAKING;
                } else if (hovered_button == 2) {
                    screen_state = STATE_MENU;
                }
            }
        }

        EndDrawing();
    }

    if (net.connected) close(net.sock);
    game_free(game);
    unload_piece_textures(&textures);
    CloseWindow();
    return 0;
}
