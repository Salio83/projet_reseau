#include "raylib.h"
#include "chess.h"
#include "render.h"

typedef enum {
    STATE_MENU,
    STATE_PLAYING,
    STATE_GAME_OVER
} GameScreenState;

int main(void)
{
    const int screenWidth = 1000;
    const int screenHeight = 900;

    InitWindow(screenWidth, screenHeight, "Chess - 2 Player Game");
    SetTargetFPS(60);

    PieceTextures textures = {0};
    load_piece_textures(&textures);

    GameState* game = game_init();
    GameScreenState screen_state = STATE_MENU;
    int hovered_button = -1;

    int board_x = (screenWidth - 8 * 80) / 2;
    int board_y = 50;
    int square_size = 80;

    while (!WindowShouldClose())
    {
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
        }
        else if (screen_state == STATE_PLAYING) {
            ClearBackground((Color){50, 50, 50, 255});

            render_board(game, board_x, board_y, square_size, &textures);
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
                        } else if (game_make_move(game, game->selected_row, game->selected_col, row, col)) {
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
        }
        else if (screen_state == STATE_GAME_OVER) {
            ClearBackground((Color){50, 50, 50, 255});
            render_board(game, board_x, board_y, square_size, &textures);
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