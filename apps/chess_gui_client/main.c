#include <arpa/inet.h>
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "chess.h"
#include "packet_types.h"
#include "raylib.h"
#include "render.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6767
#define SCREEN_W 1000
#define SCREEN_H 900
#define SQUARE_SIZE 80

typedef enum {
  STATE_MENU,
  STATE_LOBBY,
  STATE_PLAYING,
  STATE_GAME_OVER
} AppScreenState;

typedef struct {
  GameState display_state;
  uint32_t game_id;
  uint8_t my_color;
  AppScreenState screen;
  pthread_mutex_t lock;
  int sock;
  volatile int running;
  char username[32];
  char status_msg[128];
} ClientApp;

/* -------------------------------------------------------------------------
 * Parsing FEN → board[8][8]
 * Format reçu du serveur : "RNBQKBNR/PPPPPPPP/8/8/8/8/pppppppp/rnbqkbnr"
 * Majuscule = blanc, minuscule = noir, chiffre = N cases vides
 * -------------------------------------------------------------------------*/
static PieceType fen_char_to_type(char c) {
  switch (tolower(c)) {
  case 'p':
    return PAWN;
  case 'n':
    return KNIGHT;
  case 'b':
    return BISHOP;
  case 'r':
    return ROOK;
  case 'q':
    return QUEEN;
  case 'k':
    return KING;
  default:
    return PIECE_NONE;
  }
}

static void parse_fen_to_board(GameState *gs, const char *fen) {
  memset(gs->board, 0, sizeof(gs->board));
  int row = 0, col = 0;

  for (int i = 0; fen[i] != '\0' && row < 8; i++) {
    char c = fen[i];

    if (c == '/') {
      row++;
      col = 0;
    } else if (isdigit(c)) {
      col += c - '0';
    } else if (col < 8) {
      gs->board[row][col] = (Piece){
          .type = fen_char_to_type(c),
          .color = isupper(c) ? PLAYER_WHITE : PLAYER_BLACK,
      };
      col++;
    }
  }
}

/* -------------------------------------------------------------------------
 * Thread réseau : lit les paquets TCP et met à jour l'état partagé
 * -------------------------------------------------------------------------*/
static void *network_thread(void *arg) {
  ClientApp *app = (ClientApp *)arg;
  char buffer[2048];

  while (app->running) {
    int n = read(app->sock, buffer, sizeof(buffer));
    if (n <= 0) {
      pthread_mutex_lock(&app->lock);
      snprintf(app->status_msg, sizeof(app->status_msg),
               "Déconnecté du serveur.");
      app->screen = STATE_MENU;
      app->running = 0;
      pthread_mutex_unlock(&app->lock);
      break;
    }

    PacketHeader *hdr = (PacketHeader *)buffer;
    char *payload = buffer + sizeof(PacketHeader);

    pthread_mutex_lock(&app->lock);

    switch (hdr->type) {
    case PACKET_AUTH_REQ: {
      /* Le serveur renvoie PACKET_AUTH_REQ comme confirmation auth */
      app->screen = STATE_LOBBY;
      snprintf(
          app->status_msg, sizeof(app->status_msg),
          "Authentifié. Cliquez sur 'Rejoindre' pour chercher une partie.");
      break;
    }
    case PACKET_GAME_STARTED: {
      GameStarted *gs = (GameStarted *)payload;
      app->game_id = gs->game_id;
      app->my_color = gs->your_color;
      app->screen = STATE_PLAYING;
      game_reset(&app->display_state);
      snprintf(app->status_msg, sizeof(app->status_msg),
               "Partie %u démarrée ! Vous jouez les %s.", gs->game_id,
               gs->your_color == 0 ? "BLANCS" : "NOIRS");
      break;
    }
    case PACKET_GAME_STATE_UDP: {
      GameStateUDP *gsu = (GameStateUDP *)payload;
      parse_fen_to_board(&app->display_state, gsu->fen_board);
      app->display_state.current_player =
          (gsu->current_turn == 0) ? PLAYER_WHITE : PLAYER_BLACK;
      /* Détection mat/pat via absence de roi */
      int has_white_king = 0, has_black_king = 0;
      for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
          Piece p = app->display_state.board[r][c];
          if (p.type == KING) {
            if (p.color == PLAYER_WHITE)
              has_white_king = 1;
            else
              has_black_king = 1;
          }
        }
      if (!has_white_king || !has_black_king) {
        app->display_state.checkmate = 1;
        app->screen = STATE_GAME_OVER;
      }
      if (app->display_state.current_player == (PlayerColor)app->my_color)
        snprintf(app->status_msg, sizeof(app->status_msg), "A votre tour !");
      else
        snprintf(app->status_msg, sizeof(app->status_msg),
                 "Tour de l'adversaire...");
      break;
    }
    case PACKET_MOVE_ERROR: {
      snprintf(app->status_msg, sizeof(app->status_msg),
               "Coup invalide ou ce n'est pas votre tour !");
      break;
    }
    default:
      break;
    }

    pthread_mutex_unlock(&app->lock);
  }
  return NULL;
}

/* -------------------------------------------------------------------------
 * Connexion TCP au serveur
 * -------------------------------------------------------------------------*/
static int connect_to_server(const char *ip, int port) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    return -1;

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, ip, &addr.sin_addr);

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(sock);
    return -1;
  }
  return sock;
}

/* -------------------------------------------------------------------------
 * Envoi d'un paquet d'authentification
 * -------------------------------------------------------------------------*/
static void send_auth(ClientApp *app) {
  PacketHeader h = {PACKET_AUTH_REQ, sizeof(PacketHeader) + sizeof(AuthRequest),
                    0};
  AuthRequest req;
  memset(&req, 0, sizeof(req));
  strncpy(req.username, app->username, 31);
  strncpy(req.password_hash, "hash", 64);
  send(app->sock, &h, sizeof(h), 0);
  send(app->sock, &req, sizeof(req), 0);
}

/* -------------------------------------------------------------------------
 * Envoi d'une demande de matchmaking
 * -------------------------------------------------------------------------*/
static void send_join(ClientApp *app) {
  PacketHeader h = {PACKET_MATCHMAKING_REQ, sizeof(PacketHeader), 0};
  send(app->sock, &h, sizeof(h), 0);
  snprintf(app->status_msg, sizeof(app->status_msg),
           "En attente d'un adversaire...");
}

/* -------------------------------------------------------------------------
 * Envoi d'un coup
 * -------------------------------------------------------------------------*/
static void send_move(ClientApp *app, int from_row, int from_col, int to_row,
                      int to_col) {
  PacketHeader h = {PACKET_PLAYER_MOVE,
                    sizeof(PacketHeader) + sizeof(PlayerMove), 0};
  PlayerMove mv;
  mv.game_id = app->game_id;
  mv.from_square[0] = 'a' + from_col;
  mv.from_square[1] = '0' + (8 - from_row);
  mv.from_square[2] = '\0';
  mv.to_square[0] = 'a' + to_col;
  mv.to_square[1] = '0' + (8 - to_row);
  mv.to_square[2] = '\0';
  mv.promotion = '\0';
  send(app->sock, &h, sizeof(h), 0);
  send(app->sock, &mv, sizeof(mv), 0);
}

/* -------------------------------------------------------------------------
 * Rendu : écran menu (saisie username + bouton Connect)
 * -------------------------------------------------------------------------*/
static void render_connect_screen(ClientApp *app, char *input_buf,
                                  int *input_len) {
  ClearBackground((Color){20, 20, 40, 255});

  DrawText("CHESS EN RÉSEAU", SCREEN_W / 2 - 220, 150, 60, GOLD);
  DrawText("Nom d'utilisateur :", SCREEN_W / 2 - 200, 320, 28, LIGHTGRAY);

  Rectangle input_rect = {SCREEN_W / 2 - 200, 360, 400, 50};
  DrawRectangleRec(input_rect, (Color){40, 40, 60, 255});
  DrawRectangleLinesEx(input_rect, 2, GRAY);
  DrawText(input_buf, (int)input_rect.x + 10, (int)input_rect.y + 12, 28,
           WHITE);

  /* Saisie clavier */
  int key = GetCharPressed();
  while (key > 0) {
    if (key >= 32 && key <= 126 && *input_len < 31) {
      input_buf[(*input_len)++] = (char)key;
      input_buf[*input_len] = '\0';
    }
    key = GetCharPressed();
  }
  if (IsKeyPressed(KEY_BACKSPACE) && *input_len > 0) {
    input_buf[--(*input_len)] = '\0';
  }

  Rectangle btn = {SCREEN_W / 2 - 100, 440, 200, 55};
  Vector2 mouse = GetMousePosition();
  bool hover = CheckCollisionPointRec(mouse, btn);
  DrawRectangleRec(btn, hover ? DARKBLUE : (Color){30, 30, 60, 255});
  DrawRectangleLinesEx(btn, 2, hover ? GOLD : WHITE);
  DrawText("CONNECTER", (int)btn.x + 20, (int)btn.y + 14, 26,
           hover ? GOLD : WHITE);

  DrawText(app->status_msg, SCREEN_W / 2 - 300, SCREEN_H - 60, 20, ORANGE);

  if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && *input_len > 0) {
    strncpy(app->username, input_buf, 31);
    int sock = connect_to_server(SERVER_IP, SERVER_PORT);
    if (sock < 0) {
      snprintf(app->status_msg, sizeof(app->status_msg),
               "Impossible de joindre %s:%d", SERVER_IP, SERVER_PORT);
    } else {
      app->sock = sock;
      app->running = 1;
      pthread_t tid;
      pthread_create(&tid, NULL, network_thread, app);
      pthread_detach(tid);
      send_auth(app);
      snprintf(app->status_msg, sizeof(app->status_msg),
               "Connexion en cours...");
    }
  }
}

/* -------------------------------------------------------------------------
 * Rendu : écran lobby (en attente de partie)
 * -------------------------------------------------------------------------*/
static void render_lobby_screen(ClientApp *app) {
  ClearBackground((Color){20, 30, 20, 255});

  DrawText("LOBBY", SCREEN_W / 2 - 80, 150, 60, GREEN);
  DrawText(TextFormat("Connecté en tant que : %s", app->username),
           SCREEN_W / 2 - 220, 270, 26, LIGHTGRAY);

  Rectangle btn = {SCREEN_W / 2 - 130, 360, 260, 60};
  Vector2 mouse = GetMousePosition();
  bool hover = CheckCollisionPointRec(mouse, btn);
  DrawRectangleRec(btn, hover ? DARKGREEN : (Color){30, 60, 30, 255});
  DrawRectangleLinesEx(btn, 2, hover ? GOLD : WHITE);
  DrawText("REJOINDRE UNE PARTIE", (int)btn.x + 10, (int)btn.y + 16, 22,
           hover ? GOLD : WHITE);

  DrawText(app->status_msg, SCREEN_W / 2 - 300, SCREEN_H - 60, 20, YELLOW);

  if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    send_join(app);
  }
}

/* -------------------------------------------------------------------------
 * Rendu : écran de jeu (plateau + infos)
 * -------------------------------------------------------------------------*/
static void render_playing_screen(ClientApp *app, PieceTextures *textures) {
  ClearBackground((Color){50, 50, 50, 255});

  int board_x = (SCREEN_W - 8 * SQUARE_SIZE) / 2;
  int board_y = 50;

  pthread_mutex_lock(&app->lock);
  GameState local_state = app->display_state;
  uint8_t my_color = app->my_color;
  pthread_mutex_unlock(&app->lock);

  bool flipped = (my_color == 1);
  render_board(&local_state, board_x, board_y, SQUARE_SIZE, flipped, textures);

  /* Infos joueur */
  const char *color_str = (my_color == 0) ? "BLANCS" : "NOIRS";
  DrawText(TextFormat("Vous jouez : %s", color_str), 20, 10, 22, GOLD);
  const char *turn_str =
      (local_state.current_player == PLAYER_WHITE) ? "Blancs" : "Noirs";
  DrawText(TextFormat("Tour : %s", turn_str), 20, 36, 20, WHITE);

  pthread_mutex_lock(&app->lock);
  DrawText(app->status_msg, 20, SCREEN_H - 50, 18, ORANGE);
  pthread_mutex_unlock(&app->lock);

  /* Clic souris pour jouer */
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    Vector2 mouse = GetMousePosition();
    int screen_col = ((int)mouse.x - board_x) / SQUARE_SIZE;
    int screen_row = ((int)mouse.y - board_y) / SQUARE_SIZE;
    int col = flipped ? (7 - screen_col) : screen_col;
    int row = flipped ? (7 - screen_row) : screen_row;

    if (row >= 0 && row < 8 && col >= 0 && col < 8) {
      pthread_mutex_lock(&app->lock);
      bool my_turn =
          (app->display_state.current_player == (PlayerColor)app->my_color);

      if (app->display_state.selected_row == -1) {
        Piece p = app->display_state.board[row][col];
        if (my_turn && p.type != PIECE_NONE &&
            p.color == (PlayerColor)my_color) {
          app->display_state.selected_row = row;
          app->display_state.selected_col = col;
        }
      } else {
        int fr = app->display_state.selected_row;
        int fc = app->display_state.selected_col;
        if (fr == row && fc == col) {
          /* Désélection */
          app->display_state.selected_row = -1;
          app->display_state.selected_col = -1;
        } else {
          /* Envoi du coup au serveur */
          send_move(app, fr, fc, row, col);
          app->display_state.selected_row = -1;
          app->display_state.selected_col = -1;
        }
      }
      pthread_mutex_unlock(&app->lock);
    }
  }
}

/* -------------------------------------------------------------------------
 * Rendu : écran fin de partie
 * -------------------------------------------------------------------------*/
static void render_game_over_screen_net(ClientApp *app) {
  ClearBackground((Color){50, 50, 50, 255});

  int cx = SCREEN_W / 2;
  DrawText("PARTIE TERMINÉE", cx - 240, 200, 60, RED);
  DrawText(app->status_msg, cx - 300, 300, 22, GOLD);

  Rectangle btn = {cx - 120, 400, 240, 60};
  Vector2 mouse = GetMousePosition();
  bool hover = CheckCollisionPointRec(mouse, btn);
  DrawRectangleRec(btn, hover ? DARKBLUE : (Color){30, 30, 60, 255});
  DrawRectangleLinesEx(btn, 2, hover ? GOLD : WHITE);
  DrawText("REJOUER", (int)btn.x + 50, (int)btn.y + 16, 28,
           hover ? GOLD : WHITE);

  if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    pthread_mutex_lock(&app->lock);
    app->screen = STATE_LOBBY;
    game_reset(&app->display_state);
    snprintf(app->status_msg, sizeof(app->status_msg),
             "Cliquez sur 'Rejoindre' pour une nouvelle partie.");
    pthread_mutex_unlock(&app->lock);
  }
}

/* -------------------------------------------------------------------------
 * main
 * -------------------------------------------------------------------------*/
int main(void) {
  ClientApp app;
  memset(&app, 0, sizeof(app));
  pthread_mutex_init(&app.lock, NULL);
  app.sock = -1;
  app.screen = STATE_MENU;
  app.display_state.selected_row = -1;
  app.display_state.selected_col = -1;
  snprintf(app.status_msg, sizeof(app.status_msg),
           "Entrez un pseudo et connectez-vous.");

  InitWindow(SCREEN_W, SCREEN_H, "Chess - Client Réseau");
  SetTargetFPS(60);

  PieceTextures textures = {0};
  load_piece_textures(&textures);

  char input_buf[32] = {0};
  int input_len = 0;

  while (!WindowShouldClose()) {
    BeginDrawing();

    pthread_mutex_lock(&app.lock);
    AppScreenState current_screen = app.screen;
    pthread_mutex_unlock(&app.lock);

    switch (current_screen) {
    case STATE_MENU:
      render_connect_screen(&app, input_buf, &input_len);
      break;
    case STATE_LOBBY:
      render_lobby_screen(&app);
      break;
    case STATE_PLAYING:
      render_playing_screen(&app, &textures);
      break;
    case STATE_GAME_OVER:
      render_game_over_screen_net(&app);
      break;
    }

    EndDrawing();
  }

  app.running = 0;
  if (app.sock >= 0)
    close(app.sock);
  unload_piece_textures(&textures);
  pthread_mutex_destroy(&app.lock);
  CloseWindow();
  return 0;
}
