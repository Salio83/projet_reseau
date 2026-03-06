#ifndef RENDER_H
#define RENDER_H

#include "chess.h"
#include "raylib.h"

typedef struct {
    Texture2D pawn[2];
    Texture2D knight[2];
    Texture2D bishop[2];
    Texture2D rook[2];
    Texture2D queen[2];
    Texture2D king[2];
} PieceTextures;

void load_piece_textures(PieceTextures* textures);
void unload_piece_textures(PieceTextures* textures);
void render_board(GameState* game, int board_x, int board_y, int square_size, PieceTextures* textures);
void render_menu(int screen_width, int screen_height, int* hovered_button);
void render_game_over_screen(int screen_width, int screen_height, GameState* game, int* hovered_button);
void render_ui_info(GameState* game, int screen_width, int screen_height);

#endif
