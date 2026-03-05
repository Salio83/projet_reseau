#ifndef CHESS_H
#define CHESS_H

#include <stdbool.h>

typedef enum {
    PIECE_NONE = 0,
    PAWN = 1,
    KNIGHT = 2,
    BISHOP = 3,
    ROOK = 4,
    QUEEN = 5,
    KING = 6
} PieceType;

typedef enum {
    PLAYER_WHITE = 0,
    PLAYER_BLACK = 1
} PlayerColor;

typedef struct {
    PieceType type;
    PlayerColor color;
} Piece;

typedef struct {
    Piece board[8][8];
    PlayerColor current_player;
    int move_count;
    bool in_check;
    bool checkmate;
    bool stalemate;
    int selected_row;
    int selected_col;
    int last_from_row;
    int last_from_col;
    int last_to_row;
    int last_to_col;
    bool white_king_moved;
    bool white_rook_qs_moved;
    bool white_rook_ks_moved;
    bool black_king_moved;
    bool black_rook_qs_moved;
    bool black_rook_ks_moved;
    int en_passant_target_row;
    int en_passant_target_col;
} GameState;

GameState* game_init(void);
void game_free(GameState* game);
void game_reset(GameState* game);

bool game_is_valid_move(GameState* game, int from_row, int from_col, int to_row, int to_col);
bool game_make_move(GameState* game, int from_row, int from_col, int to_row, int to_col);
void game_update_state(GameState* game);

bool game_is_in_check(GameState* game, PlayerColor color);
bool game_is_checkmate(GameState* game, PlayerColor color);
bool game_is_stalemate(GameState* game, PlayerColor color);

bool game_has_legal_moves(GameState* game, PlayerColor color);

#endif
