#include "chess.h"
#include "raylib.h"
#include "render.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define TCP_PORT 25565

typedef enum { STATE_MENU, STATE_PLAYING, STATE_GAME_OVER } GameScreenState;

/*
 * On fait un serveru TCP pour écouter les requêtes .
 * On ouvre un socket ipv4 en mode pas bloquand, et on écoute le port 6767 pour
 * le moment
 */
int init_tcp_server(int port) {
  int server_fd;
  struct sockaddr_in address;

  // Création du socket TCP
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("Échec de la création du socket");
    return -1;
  }

  // on peut réutiliser le port juste après la fermeture
  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("setsockopt a échoué");
  }

  // NONBLOCK
  int flags = fcntl(server_fd, F_GETFL, 0);
  fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

  // Préparation du serv
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port); // On passe le port en format réseau

  // Link le socket à l'adresse et au port
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("Échec du bind");
    return -1;
  }

  // On commence à écouter
  if (listen(server_fd, 3) < 0) {
    perror("Échec de l'écoute");
    return -1;
  }

  return server_fd;
}

/*
 * Chaque frame on va voir les connexions
 * Si on a une connexion, on va voir ce qu'elle nous envoie
 * on traduit le message et on tente de faire le coup
 */
void process_tcp_clients(int server_fd, GameState *game) {
  if (server_fd < 0)
    return;

  struct sockaddr_in address;
  socklen_t addrlen = sizeof(address);
  // Tente d'accepter une nouvelle connexion. Non bloquant grâce à fcntl plus
  // haut.
  int new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);

  // S'il y a un client connecté
  if (new_socket >= 0) {
    // Petit timeout pas trop long pour la lecture
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv,
               sizeof tv);

    char buffer[1024] = {0};
    // Lecture du message recu
    int valread = read(new_socket, buffer, sizeof(buffer) - 1);

    if (valread >= 4) {
      char from_file = buffer[0]; // Colonne de départ
      char from_rank = buffer[1]; // Rangée de départ
      char to_file = buffer[2];   // Colonne d'arrivée
      char to_rank = buffer[3];   // Rangée d'arrivée

      // Vérification
      if (from_file >= 'a' && from_file <= 'h' && from_rank >= '1' &&
          from_rank <= '8' && to_file >= 'a' && to_file <= 'h' &&
          to_rank >= '1' && to_rank <= '8') {

        int from_col = from_file - 'a';
        int from_row = 8 - (from_rank - '0');
        int to_col = to_file - 'a';
        int to_row = 8 - (to_rank - '0');

        // Logging au cas ou on a des problèmes
        printf("Serveur TCP : Coup reçu %c%c%c%c (de %d,%d vers %d,%d)\n",
               from_file, from_rank, to_file, to_rank, from_row, from_col,
               to_row, to_col);

        // On passe le coup au moteur.Pour l'instant on vérifie pas si il est
        // valide ou pas le jeu s'en occupe mais ne donne pas de feedback
        if (game_make_move(game, from_row, from_col, to_row, to_col)) {
          printf("Serveur TCP : Coup appliqué avec succès !\n");
        } else {
          printf("Serveur TCP : Coup invalide !\n");
        }
      }
    }
    // Fermeture de la connexion après avoir traité la requête
    close(new_socket);
  }
}

int main(void) {
  const int screenWidth = 1000;
  const int screenHeight = 900;

  InitWindow(screenWidth, screenHeight, "Chess - 2 Player Game");
  SetTargetFPS(60);

  PieceTextures textures = {0};
  load_piece_textures(&textures);

  GameState *game = game_init();
  GameScreenState screen_state = STATE_MENU;
  int hovered_button = -1;

  int board_x = (screenWidth - 8 * 80) / 2;
  int board_y = 50;
  int square_size = 80;

  int tcp_server_fd = init_tcp_server(TCP_PORT);

  while (!WindowShouldClose()) {
    BeginDrawing();

    if (screen_state == STATE_MENU) {
      render_menu(screenWidth, screenHeight, &hovered_button);

      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (hovered_button == 1) {
          screen_state = STATE_PLAYING;
          game_reset(game);
        } else if (hovered_button == 2) {
          break;
        }
      }
    } else if (screen_state == STATE_PLAYING) {
      ClearBackground((Color){50, 50, 50, 255});

      process_tcp_clients(tcp_server_fd, game);

      if (game->checkmate || game->stalemate) {
        screen_state = STATE_GAME_OVER;
      }

      render_board(game, board_x, board_y, square_size, false, &textures);
      render_ui_info(game, screenWidth, screenHeight);

      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse_pos = GetMousePosition();
        int col = (mouse_pos.x - board_x) / square_size;
        int row = (mouse_pos.y - board_y) / square_size;

        if (row >= 0 && row < 8 && col >= 0 && col < 8) {
          if (game->selected_row == -1) {
            if (game->board[row][col].type != PIECE_NONE &&
                game->board[row][col].color == game->current_player) {
              game->selected_row = row;
              game->selected_col = col;
            }
          } else {
            if (game->selected_row == row && game->selected_col == col) {
              game->selected_row = -1;
              game->selected_col = -1;
            } else if (game_make_move(game, game->selected_row,
                                      game->selected_col, row, col)) {
              game->selected_row = -1;
              game->selected_col = -1;

              if (game->checkmate || game->stalemate) {
                screen_state = STATE_GAME_OVER;
              }
            } else {
              if (game->board[row][col].type != PIECE_NONE &&
                  game->board[row][col].color == game->current_player) {
                game->selected_row = row;
                game->selected_col = col;
              } else {
                game->selected_row = -1;
                game->selected_col = -1;
              }
            }
          }
        }
      }

      if (IsKeyPressed(KEY_R)) {
        game_reset(game);
      }
    } else if (screen_state == STATE_GAME_OVER) {
      ClearBackground((Color){50, 50, 50, 255});
      render_board(game, board_x, board_y, square_size, false, &textures);
      render_ui_info(game, screenWidth, screenHeight);
      render_game_over_screen(screenWidth, screenHeight, game, &hovered_button);

      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (hovered_button == 1) {
          game_reset(game);
          screen_state = STATE_PLAYING;
        } else if (hovered_button == 2) {
          screen_state = STATE_MENU;
        }
      }
    }

    EndDrawing();
  }

  game_free(game);
  unload_piece_textures(&textures);
  CloseWindow();
  return 0;
}