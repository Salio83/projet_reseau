#include "chess.h"
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

static bool is_in_bounds(int row, int col) {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

static bool is_square_attacked(GameState* game, int row, int col, PlayerColor by_color) {
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            Piece piece = game->board[r][c];
            if (piece.type == PIECE_NONE || piece.color != by_color) continue;

            if (game_is_valid_move(game, r, c, row, col)) {
                return true;
            }
        }
    }
    return false;
}

static bool is_piece_pinned(GameState* game, int row, int col, int to_row, int to_col) {
    Piece piece = game->board[row][col];
    if (piece.color == PIECE_NONE) return false;

    Piece captured = game->board[to_row][to_col];
    
    bool is_en_passant = (piece.type == PAWN && to_row == game->en_passant_target_row && to_col == game->en_passant_target_col);
    Piece en_passant_captured = {PIECE_NONE, 0};
    int ep_cap_row = row;
    int ep_cap_col = to_col;
    
    if (is_en_passant) {
        en_passant_captured = game->board[ep_cap_row][ep_cap_col];
        game->board[ep_cap_row][ep_cap_col] = (Piece){PIECE_NONE, 0};
    }

    game->board[to_row][to_col] = piece;
    game->board[row][col] = (Piece){PIECE_NONE, 0};

    PlayerColor king_color = piece.color;
    PlayerColor enemy_color = (piece.color == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    int king_row = -1, king_col = -1;

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (game->board[r][c].type == KING && game->board[r][c].color == king_color) {
                king_row = r;
                king_col = c;
                break;
            }
        }
        if (king_row != -1) break;
    }

    bool pinned = is_square_attacked(game, king_row, king_col, enemy_color);

    game->board[row][col] = piece;
    game->board[to_row][to_col] = captured;

    if (is_en_passant) {
        game->board[ep_cap_row][ep_cap_col] = en_passant_captured;
    }

    return pinned;
}

static bool is_move_pawn(GameState* game, int from_row, int from_col, int to_row, int to_col) {
    Piece piece = game->board[from_row][from_col];
    if (piece.type != PAWN) return false;

    int direction = (piece.color == PLAYER_WHITE) ? -1 : 1;
    int start_row = (piece.color == PLAYER_WHITE) ? 6 : 1;

    if (game->board[to_row][to_col].type == PIECE_NONE) {
        if (to_col == from_col && to_row == from_row + direction) {
            return true;
        }
        if (to_col == from_col && to_row == from_row + 2 * direction && from_row == start_row &&
            game->board[from_row + direction][from_col].type == PIECE_NONE) {
            return true;
        }
        if (to_row == from_row + direction && abs(to_col - from_col) == 1 &&
            to_row == game->en_passant_target_row && to_col == game->en_passant_target_col) {
            return true;
        }
    } else if (to_row == from_row + direction && abs(to_col - from_col) == 1) {
        return game->board[to_row][to_col].color != piece.color;
    }
    return false;
}

static bool is_move_knight(int from_row, int from_col, int to_row, int to_col) {
    int dr = abs(to_row - from_row);
    int dc = abs(to_col - from_col);
    return (dr == 2 && dc == 1) || (dr == 1 && dc == 2);
}

static bool is_move_bishop(GameState* game, int from_row, int from_col, int to_row, int to_col) {
    if (abs(to_row - from_row) != abs(to_col - from_col)) return false;

    int dr = (to_row > from_row) ? 1 : -1;
    int dc = (to_col > from_col) ? 1 : -1;
    int r = from_row + dr;
    int c = from_col + dc;

    while (r != to_row || c != to_col) {
        if (game->board[r][c].type != PIECE_NONE) return false;
        r += dr;
        c += dc;
    }
    return true;
}

static bool is_move_rook(GameState* game, int from_row, int from_col, int to_row, int to_col) {
    if (from_row != to_row && from_col != to_col) return false;

    int dr = 0, dc = 0;
    if (to_row > from_row) dr = 1;
    else if (to_row < from_row) dr = -1;
    if (to_col > from_col) dc = 1;
    else if (to_col < from_col) dc = -1;

    int r = from_row + dr;
    int c = from_col + dc;

    while (r != to_row || c != to_col) {
        if (game->board[r][c].type != PIECE_NONE) return false;
        r += dr;
        c += dc;
    }
    return true;
}

static bool is_move_queen(GameState* game, int from_row, int from_col, int to_row, int to_col) {
    return is_move_rook(game, from_row, from_col, to_row, to_col) ||
           is_move_bishop(game, from_row, from_col, to_row, to_col);
}

static bool is_move_king(GameState* game, int from_row, int from_col, int to_row, int to_col) {
    if (abs(to_row - from_row) <= 1 && abs(to_col - from_col) <= 1 &&
        (to_row != from_row || to_col != from_col)) {
        return true;
    }

    Piece piece = game->board[from_row][from_col];
    if (piece.type != KING || from_row != to_row) return false;
    if (game_is_in_check(game, piece.color)) return false;

    if (piece.color == PLAYER_WHITE) {
        if (game->white_king_moved) return false;
        
        if (to_col == 6 && !game->white_rook_ks_moved) {
            if (game->board[7][5].type == PIECE_NONE && game->board[7][6].type == PIECE_NONE) {
                if (!is_square_attacked(game, 7, 5, PLAYER_BLACK) && !is_square_attacked(game, 7, 6, PLAYER_BLACK)) {
                    return true;
                }
            }
        }
        if (to_col == 2 && !game->white_rook_qs_moved) {
            if (game->board[7][1].type == PIECE_NONE && game->board[7][2].type == PIECE_NONE && game->board[7][3].type == PIECE_NONE) {
                if (!is_square_attacked(game, 7, 3, PLAYER_BLACK) && !is_square_attacked(game, 7, 2, PLAYER_BLACK)) {
                    return true;
                }
            }
        }
    } else {
        if (game->black_king_moved) return false;
        
        if (to_col == 6 && !game->black_rook_ks_moved) {
            if (game->board[0][5].type == PIECE_NONE && game->board[0][6].type == PIECE_NONE) {
                if (!is_square_attacked(game, 0, 5, PLAYER_WHITE) && !is_square_attacked(game, 0, 6, PLAYER_WHITE)) {
                    return true;
                }
            }
        }
        if (to_col == 2 && !game->black_rook_qs_moved) {
            if (game->board[0][1].type == PIECE_NONE && game->board[0][2].type == PIECE_NONE && game->board[0][3].type == PIECE_NONE) {
                if (!is_square_attacked(game, 0, 3, PLAYER_WHITE) && !is_square_attacked(game, 0, 2, PLAYER_WHITE)) {
                    return true;
                }
            }
        }
    }
    return false;
}

GameState* game_init(void) {
    GameState* game = (GameState*)malloc(sizeof(GameState));
    game_reset(game);
    return game;
}

void game_free(GameState* game) {
    free(game);
}

void game_reset(GameState* game) {
    memset(game->board, 0, sizeof(game->board));

    for (int i = 0; i < 8; i++) {
        game->board[1][i] = (Piece){PAWN, PLAYER_BLACK};
        game->board[6][i] = (Piece){PAWN, PLAYER_WHITE};
    }

    game->board[0][0] = (Piece){ROOK, PLAYER_BLACK};
    game->board[0][1] = (Piece){KNIGHT, PLAYER_BLACK};
    game->board[0][2] = (Piece){BISHOP, PLAYER_BLACK};
    game->board[0][3] = (Piece){QUEEN, PLAYER_BLACK};
    game->board[0][4] = (Piece){KING, PLAYER_BLACK};
    game->board[0][5] = (Piece){BISHOP, PLAYER_BLACK};
    game->board[0][6] = (Piece){KNIGHT, PLAYER_BLACK};
    game->board[0][7] = (Piece){ROOK, PLAYER_BLACK};

    game->board[7][0] = (Piece){ROOK, PLAYER_WHITE};
    game->board[7][1] = (Piece){KNIGHT, PLAYER_WHITE};
    game->board[7][2] = (Piece){BISHOP, PLAYER_WHITE};
    game->board[7][3] = (Piece){QUEEN, PLAYER_WHITE};
    game->board[7][4] = (Piece){KING, PLAYER_WHITE};
    game->board[7][5] = (Piece){BISHOP, PLAYER_WHITE};
    game->board[7][6] = (Piece){KNIGHT, PLAYER_WHITE};
    game->board[7][7] = (Piece){ROOK, PLAYER_WHITE};

    game->current_player = PLAYER_WHITE;
    game->move_count = 0;
    game->in_check = false;
    game->checkmate = false;
    game->stalemate = false;
    game->selected_row = -1;
    game->selected_col = -1;
    game->last_from_row = -1;
    game->last_from_col = -1;
    game->last_to_row = -1;
    game->last_to_col = -1;
    game->white_king_moved = false;
    game->white_rook_qs_moved = false;
    game->white_rook_ks_moved = false;
    game->black_king_moved = false;
    game->black_rook_qs_moved = false;
    game->black_rook_ks_moved = false;
    game->en_passant_target_row = -1;
    game->en_passant_target_col = -1;
}

bool game_is_valid_move(GameState* game, int from_row, int from_col, int to_row, int to_col) {
    if (!is_in_bounds(from_row, from_col) || !is_in_bounds(to_row, to_col)) return false;
    if (from_row == to_row && from_col == to_col) return false;

    Piece piece = game->board[from_row][from_col];
    if (piece.type == PIECE_NONE) return false;

    Piece target = game->board[to_row][to_col];
    if (target.type != PIECE_NONE && target.color == piece.color) return false;

    bool valid = false;

    switch (piece.type) {
        case PAWN:
            valid = is_move_pawn(game, from_row, from_col, to_row, to_col);
            break;
        case KNIGHT:
            valid = is_move_knight(from_row, from_col, to_row, to_col);
            break;
        case BISHOP:
            valid = is_move_bishop(game, from_row, from_col, to_row, to_col);
            break;
        case ROOK:
            valid = is_move_rook(game, from_row, from_col, to_row, to_col);
            break;
        case QUEEN:
            valid = is_move_queen(game, from_row, from_col, to_row, to_col);
            break;
        case KING:
            valid = is_move_king(game, from_row, from_col, to_row, to_col);
            break;
        default:
            return false;
    }

    if (!valid) return false;

    if (is_piece_pinned(game, from_row, from_col, to_row, to_col)) return false;

    return true;
}

bool game_make_move(GameState* game, int from_row, int from_col, int to_row, int to_col) {
    if (!game_is_valid_move(game, from_row, from_col, to_row, to_col)) return false;

    Piece piece = game->board[from_row][from_col];

    if (piece.color != game->current_player) return false;

    bool is_en_passant = (piece.type == PAWN && to_row == game->en_passant_target_row && to_col == game->en_passant_target_col);
    bool is_castling = (piece.type == KING && abs(to_col - from_col) == 2);

    int next_ep_row = -1;
    int next_ep_col = -1;

    game->board[to_row][to_col] = piece;
    game->board[from_row][from_col] = (Piece){PIECE_NONE, 0};

    if (is_en_passant) {
        game->board[from_row][to_col] = (Piece){PIECE_NONE, 0};
    } else if (is_castling) {
        if (to_col == 6) { // Kingside
            game->board[to_row][5] = game->board[to_row][7];
            game->board[to_row][7] = (Piece){PIECE_NONE, 0};
        } else if (to_col == 2) { // Queenside
            game->board[to_row][3] = game->board[to_row][0];
            game->board[to_row][0] = (Piece){PIECE_NONE, 0};
        }
    }

    if (piece.type == PAWN && abs(to_row - from_row) == 2) {
        next_ep_row = (from_row + to_row) / 2;
        next_ep_col = from_col;
    }

    if (piece.type == KING) {
        if (piece.color == PLAYER_WHITE) game->white_king_moved = true;
        else game->black_king_moved = true;
    } else if (piece.type == ROOK) {
        if (from_row == 7 && from_col == 0) game->white_rook_qs_moved = true;
        if (from_row == 7 && from_col == 7) game->white_rook_ks_moved = true;
        if (from_row == 0 && from_col == 0) game->black_rook_qs_moved = true;
        if (from_row == 0 && from_col == 7) game->black_rook_ks_moved = true;
    }

    if (to_row == 7 && to_col == 0) game->white_rook_qs_moved = true;
    if (to_row == 7 && to_col == 7) game->white_rook_ks_moved = true;
    if (to_row == 0 && to_col == 0) game->black_rook_qs_moved = true;
    if (to_row == 0 && to_col == 7) game->black_rook_ks_moved = true;

    if (piece.type == PAWN) {
        int promotion_row = (piece.color == PLAYER_WHITE) ? 0 : 7;
        if (to_row == promotion_row) {
            game->board[to_row][to_col].type = QUEEN;
        }
    }

    game->en_passant_target_row = next_ep_row;
    game->en_passant_target_col = next_ep_col;

    game->current_player = (game->current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    game->move_count++;

    game->last_from_row = from_row;
    game->last_from_col = from_col;
    game->last_to_row = to_row;
    game->last_to_col = to_col;

    game_update_state(game);

    return true;
}

void game_update_state(GameState* game) {
    game->in_check = game_is_in_check(game, game->current_player);
    game->checkmate = game_is_checkmate(game, game->current_player);
    game->stalemate = game_is_stalemate(game, game->current_player);
}

bool game_is_in_check(GameState* game, PlayerColor color) {
    int king_row = -1, king_col = -1;

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (game->board[r][c].type == KING && game->board[r][c].color == color) {
                king_row = r;
                king_col = c;
                break;
            }
        }
        if (king_row != -1) break;
    }

    if (king_row == -1) return false;

    PlayerColor enemy_color = (color == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    return is_square_attacked(game, king_row, king_col, enemy_color);
}

bool game_has_legal_moves(GameState* game, PlayerColor color) {
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            Piece piece = game->board[r][c];
            if (piece.type == PIECE_NONE || piece.color != color) continue;

            for (int tr = 0; tr < 8; tr++) {
                for (int tc = 0; tc < 8; tc++) {
                    if (game_is_valid_move(game, r, c, tr, tc)) {
                        Piece temp = game->board[tr][tc];
                        game->board[tr][tc] = piece;
                        game->board[r][c] = (Piece){PIECE_NONE, 0};

                        bool king_safe = !game_is_in_check(game, color);

                        game->board[r][c] = piece;
                        game->board[tr][tc] = temp;

                        if (king_safe) return true;
                    }
                }
            }
        }
    }
    return false;
}

bool game_is_checkmate(GameState* game, PlayerColor color) {
    return game_is_in_check(game, color) && !game_has_legal_moves(game, color);
}

bool game_is_stalemate(GameState* game, PlayerColor color) {
    return !game_is_in_check(game, color) && !game_has_legal_moves(game, color);
}
