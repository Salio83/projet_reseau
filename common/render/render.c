#include "render.h"

#include <stddef.h>
#include <stdio.h>

#include "raylib.h"

void load_piece_textures(PieceTextures* textures) {
    char path[256];
    
    snprintf(path, sizeof(path), "%s/assets/img/white_pawn.png", PROJECT_DIR);
    textures->pawn[0] = LoadTexture(path);
    snprintf(path, sizeof(path), "%s/assets/img/black_pawn.png", PROJECT_DIR);
    textures->pawn[1] = LoadTexture(path);
    
    snprintf(path, sizeof(path), "%s/assets/img/white_knight.png", PROJECT_DIR);
    textures->knight[0] = LoadTexture(path);
    snprintf(path, sizeof(path), "%s/assets/img/black_knight.png", PROJECT_DIR);
    textures->knight[1] = LoadTexture(path);
    
    snprintf(path, sizeof(path), "%s/assets/img/white_bishop.png", PROJECT_DIR);
    textures->bishop[0] = LoadTexture(path);
    snprintf(path, sizeof(path), "%s/assets/img/black_bishop.png", PROJECT_DIR);
    textures->bishop[1] = LoadTexture(path);
    
    snprintf(path, sizeof(path), "%s/assets/img/white_rook.png", PROJECT_DIR);
    textures->rook[0] = LoadTexture(path);
    snprintf(path, sizeof(path), "%s/assets/img/black_rook.png", PROJECT_DIR);
    textures->rook[1] = LoadTexture(path);
    
    snprintf(path, sizeof(path), "%s/assets/img/white_queen.png", PROJECT_DIR);
    textures->queen[0] = LoadTexture(path);
    snprintf(path, sizeof(path), "%s/assets/img/black_queen.png", PROJECT_DIR);
    textures->queen[1] = LoadTexture(path);
    
    snprintf(path, sizeof(path), "%s/assets/img/white_king.png", PROJECT_DIR);
    textures->king[0] = LoadTexture(path);
    snprintf(path, sizeof(path), "%s/assets/img/black_king.png", PROJECT_DIR);
    textures->king[1] = LoadTexture(path);
}

void unload_piece_textures(PieceTextures* textures) {
    UnloadTexture(textures->pawn[0]);
    UnloadTexture(textures->pawn[1]);
    UnloadTexture(textures->knight[0]);
    UnloadTexture(textures->knight[1]);
    UnloadTexture(textures->bishop[0]);
    UnloadTexture(textures->bishop[1]);
    UnloadTexture(textures->rook[0]);
    UnloadTexture(textures->rook[1]);
    UnloadTexture(textures->queen[0]);
    UnloadTexture(textures->queen[1]);
    UnloadTexture(textures->king[0]);
    UnloadTexture(textures->king[1]);
}

static void draw_piece(PieceType type, PlayerColor color, int x, int y, int size, PieceTextures* textures) {
    Texture2D* texture = NULL;
    int color_idx = (color == PLAYER_WHITE) ? 0 : 1;

    switch (type) {
        case PAWN:
            texture = &textures->pawn[color_idx];
            break;
        case KNIGHT:
            texture = &textures->knight[color_idx];
            break;
        case BISHOP:
            texture = &textures->bishop[color_idx];
            break;
        case ROOK:
            texture = &textures->rook[color_idx];
            break;
        case QUEEN:
            texture = &textures->queen[color_idx];
            break;
        case KING:
            texture = &textures->king[color_idx];
            break;
        default:
            return;
    }

    float scale = 64.0f / 32.0f;
    DrawTextureEx(*texture, (Vector2){x + (size - 64) / 2.0f, y + (size - 64) / 2.0f}, 0.0f, scale, WHITE);
}

void render_board(GameState* game, int board_x, int board_y, int square_size, PieceTextures* textures) {
    Color light_sq = (Color){229, 230, 234, 255};
    Color dark_sq = (Color){121, 130, 147, 255};
    Color light_last_move = (Color){220, 230, 200, 255};
    Color dark_last_move = (Color){80, 90, 100, 255};
    Color light_selected = (Color){250, 250, 250, 255};
    Color dark_selected = (Color){100, 100, 100, 255};

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int x = board_x + col * square_size;
            int y = board_y + row * square_size;

            bool is_light = (row + col) % 2 == 0;
            bool is_last_move = (game->last_from_row == row && game->last_from_col == col) ||
                                (game->last_to_row == row && game->last_to_col == col);
            bool is_selected = (game->selected_row == row && game->selected_col == col);

            Color sq_color;
            if (is_selected) {
                sq_color = is_light ? light_selected : dark_selected;
            } else if (is_last_move) {
                sq_color = is_light ? light_last_move : dark_last_move;
            } else {
                sq_color = is_light ? light_sq : dark_sq;
            }

            DrawRectangle(x, y, square_size, square_size, sq_color);

            Piece piece = game->board[row][col];
            if (piece.type != PIECE_NONE) {
                draw_piece(piece.type, piece.color, x, y, square_size, textures);
            }
        }
    }

    DrawRectangleLines(board_x, board_y, 8 * square_size, 8 * square_size, BLACK);
}

void render_menu(int screen_width, int screen_height, int* hovered_button) {
    ClearBackground(DARKBLUE);

    DrawText("CHESS", screen_width/2 - 150, screen_height/4, 80, GOLD);

    int button_width = 300;
    int button_height = 60;
    int button_x = screen_width/2 - button_width/2;
    int button_y = screen_height/2;
    int button_spacing = 100;

    Vector2 mouse_pos = GetMousePosition();

    Rectangle play_rect = {button_x, button_y, button_width, button_height};
    *hovered_button = CheckCollisionPointRec(mouse_pos, play_rect) ? 1 : -1;

    Color play_color = (*hovered_button == 1) ? YELLOW : WHITE;
    DrawRectangle(button_x, button_y, button_width, button_height, DARKGRAY);
    DrawRectangleLines(button_x, button_y, button_width, button_height, play_color);
    DrawText("PLAY", button_x + button_width/2 - 60, button_y + button_height/2 - 20, 40, play_color);

    Rectangle quit_rect = {button_x, button_y + button_spacing, button_width, button_height};
    Color quit_color = CheckCollisionPointRec(mouse_pos, quit_rect) ? YELLOW : WHITE;
    *hovered_button = CheckCollisionPointRec(mouse_pos, quit_rect) ? 2 : *hovered_button;
    
    DrawRectangle(button_x, button_y + button_spacing, button_width, button_height, DARKGRAY);
    DrawRectangleLines(button_x, button_y + button_spacing, button_width, button_height, quit_color);
    DrawText("QUIT", button_x + button_width/2 - 50, button_y + button_spacing + button_height/2 - 20, 40, quit_color);
}

void render_game_over_screen(int screen_width, int screen_height, GameState* game, int* hovered_button) {
    DrawRectangle(0, 0, screen_width, screen_height, (Color){0, 0, 0, 150});

    int modal_width = 500;
    int modal_height = 400;
    int modal_x = screen_width / 2 - modal_width / 2;
    int modal_y = screen_height / 2 - modal_height / 2;

    DrawRectangle(modal_x, modal_y, modal_width, modal_height, (Color){30, 30, 40, 255});
    DrawRectangleLines(modal_x, modal_y, modal_width, modal_height, GOLD);

    const char* title = game->checkmate ? "CHECKMATE!" : "STALEMATE!";
    const char* winner = "";

    if (game->checkmate) {
        PlayerColor winner_color = (game->current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
        winner = (winner_color == PLAYER_WHITE) ? "WHITE WINS!" : "BLACK WINS!";
    } else {
        winner = "DRAW";
    }

    int title_width = MeasureText(title, 60);
    DrawText(title, screen_width / 2 - title_width / 2, modal_y + 40, 60, RED);

    int winner_width = MeasureText(winner, 50);
    DrawText(winner, screen_width / 2 - winner_width / 2, modal_y + 120, 50, GOLD);

    int button_width = 250;
    int button_height = 60;
    int button_x = screen_width / 2 - button_width / 2;
    int button_y = modal_y + 220;

    Vector2 mouse_pos = GetMousePosition();

    Rectangle play_again_rect = {button_x, button_y, button_width, button_height};
    Color play_color = CheckCollisionPointRec(mouse_pos, play_again_rect) ? GOLD : WHITE;
    *hovered_button = CheckCollisionPointRec(mouse_pos, play_again_rect) ? 1 : -1;

    DrawRectangle(button_x, button_y, button_width, button_height, (Color){50, 50, 60, 255});
    DrawRectangleLines(button_x, button_y, button_width, button_height, play_color);
    int play_text_width = MeasureText("PLAY AGAIN", 30);
    DrawText("PLAY AGAIN", screen_width / 2 - play_text_width / 2, button_y + button_height / 2 - 15, 30, play_color);

    Rectangle menu_rect = {button_x, button_y + 80, button_width, button_height};
    Color menu_color = CheckCollisionPointRec(mouse_pos, menu_rect) ? GOLD : WHITE;
    *hovered_button = CheckCollisionPointRec(mouse_pos, menu_rect) ? 2 : *hovered_button;

    DrawRectangle(button_x, button_y + 80, button_width, button_height, (Color){50, 50, 60, 255});
    DrawRectangleLines(button_x, button_y + 80, button_width, button_height, menu_color);
    int menu_text_width = MeasureText("MENU", 30);
    DrawText("MENU", screen_width / 2 - menu_text_width / 2, button_y + 80 + button_height / 2 - 15, 30, menu_color);
}

void render_ui_info(GameState* game, int screen_width, int screen_height) {
    int info_y = 20;
    
    const char* current_player = (game->current_player == PLAYER_WHITE) ? "WHITE" : "BLACK";
    DrawText(TextFormat("Current: %s", current_player), 20, info_y, 24, 
            (game->current_player == PLAYER_WHITE) ? WHITE : (Color){100, 100, 100, 255});

    if (game->checkmate) {
        DrawText("CHECKMATE!", 20, screen_height - 40, 24, RED);
    } else if (game->stalemate) {
        DrawText("STALEMATE!", 20, screen_height - 40, 24, YELLOW);
    } else if (game->in_check) {
        DrawText("CHECK!", 20, screen_height - 40, 24, ORANGE);
    }

    DrawText("Click piece to select, click square to move", 20, screen_height - 70, 16, LIGHTGRAY);
    DrawText("R to reset game", screen_width - 200, screen_height - 40, 16, LIGHTGRAY);
}
