#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "../../common/ipc_utils/ipc_utils.h"
#include "../../common/ipc_utils/ipc_keys.h"
#include "../../common/network_models/packet_types.h"
#include "../../common/chess_engine/chess.h"

/**
 * @file game_worker_main.c
 * @brief Service de gestion de la logique de jeu (Moteur d'échecs).
 * 
 * Ce service tourne en arrière-plan, reçoit les coups des joueurs via IPC,
 * les valide avec le moteur d'échecs, et renvoie l'état mis à jour du plateau.
 */

#define MAX_GAMES 50

// Structure représentant une partie d'échecs en cours sur le serveur
typedef struct {
    uint32_t game_id;       // Identifiant unique de la partie
    uint32_t player_white;  // Client ID (fd) du joueur blanc
    uint32_t player_black;  // Client ID (fd) du joueur noir
    GameState* state;       // État interne du moteur d'échecs (board, tour, etc.)
    bool active;            // Si cet emplacement est utilisé
} GameInstance;

// Tableau global stockant toutes les parties gérées par ce worker
GameInstance games[MAX_GAMES];

/**
 * @brief Recherche l'indice d'une partie dans le tableau global par son ID.
 */
int find_game(uint32_t gid) {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].active && games[i].game_id == gid) return i;
    }
    return -1;
}

/**
 * @brief Convertit une position "e2" en indices de matrice (row, col).
 */
void pos_to_coords(const char* pos, int* row, int* col) {
    *col = pos[0] - 'a';
    *row = 8 - (pos[1] - '0');
}

/**
 * @brief Sérialise le plateau d'échecs en une chaîne de caractères simple.
 * Permet un transfert léger sur le réseau (format proche du FEN).
 */
void board_to_simple_string(GameState* state, char* buffer) {
    int pos = 0;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            Piece p = state->board[r][c];
            if (p.type == PIECE_NONE) buffer[pos++] = '.';
            else {
                char base = ' ';
                switch(p.type) {
                    case PAWN: base = 'P'; break;
                    case KNIGHT: base = 'N'; break;
                    case BISHOP: base = 'B'; break;
                    case ROOK: base = 'R'; break;
                    case QUEEN: base = 'Q'; break;
                    case KING: base = 'K'; break;
                    default: base = '?';
                }
                // Blancs en majuscules, Noirs en minuscules
                buffer[pos++] = (p.color == PLAYER_WHITE) ? base : (base + 32); 
            }
        }
        buffer[pos++] = '/';
    }
    buffer[pos] = '\0';
}

int main() {
    printf("[GameWorker] Démarrage...\n");

    // Connexion aux files de messages : Entrée (gameworker) et Sortie (gateway)
    int gameworker_mq = ipc_msg_get(ipc_get_key(GAMEWORKER_MSG_QUEUE_PATH, GAMEWORKER_MSG_QUEUE_ID));
    int gateway_mq = ipc_msg_get(ipc_get_key(GATEWAY_MSG_QUEUE_PATH, GATEWAY_MSG_QUEUE_ID));

    for (int i = 0; i < MAX_GAMES; i++) games[i].active = false;

    char msg_buffer[MAX_MSG_SIZE];

    // Boucle infinie d'écoute des messages IPC
    while (1) {
        // Lecture bloquante des messages envoyés par le Gateway ou le Matchmaker
        int nbytes = ipc_msg_receive(gameworker_mq, msg_buffer, MAX_MSG_SIZE, 1);
        if (nbytes > 0) {
            PacketHeader* header = (PacketHeader*)msg_buffer;

            // CAS 1 : Notification de début de partie (provient du Matchmaker)
            if (header->type == PACKET_GAME_STARTED) {
                GameStarted* gs = (GameStarted*)(msg_buffer + sizeof(PacketHeader));
                printf("[GameWorker] Création de la partie %d\n", gs->game_id);

                for (int i = 0; i < MAX_GAMES; i++) {
                    if (!games[i].active) {
                        games[i].active = true;
                        games[i].game_id = gs->game_id;
                        games[i].player_white = header->client_id;
                        games[i].player_black = gs->opponent_id;
                        // Initialisation du moteur d'échecs pour cette nouvelle partie
                        games[i].state = game_init();
                        printf("[GameWorker] Game %d: Blanc=%d, Noir=%d\n", gs->game_id, games[i].player_white, games[i].player_black);
                        break;
                    }
                }
            } 
            // CAS 2 : Réception d'un coup envoyé par un joueur (via Gateway)
            else if (header->type == PACKET_PLAYER_MOVE) {
                PlayerMove* move = (PlayerMove*)(msg_buffer + sizeof(PacketHeader));
                int g_idx = find_game(move->game_id);
                
                if (g_idx != -1) {
                    // Identification du joueur dont c'est le tour
                    uint32_t current_player_id = (games[g_idx].state->current_player == PLAYER_WHITE) 
                                                 ? games[g_idx].player_white : games[g_idx].player_black;

                    // 1. Sécurité : Vérifier que c'est bien le tour de ce joueur
                    if (header->client_id != current_player_id) {
                        printf("[GameWorker] Erreur: Joueur %d tente de jouer alors que c'est le tour de %d\n", header->client_id, current_player_id);
                        
                        PacketHeader err_h = {PACKET_MOVE_ERROR, sizeof(PacketHeader), header->client_id};
                        ipc_msg_send(gateway_mq, &err_h, sizeof(err_h), 1);
                        continue;
                    }

                    int from_r, from_c, to_r, to_c;
                    pos_to_coords(move->from_square, &from_r, &from_c);
                    pos_to_coords(move->to_square, &to_r, &to_c);

                    // 2. Validation et exécution du coup via le moteur d'échecs
                    if (game_make_move(games[g_idx].state, from_r, from_c, to_r, to_c)) {
                        printf("[GameWorker] Game %d: Coup réussi %s->%s\n", move->game_id, move->from_square, move->to_square);
                        
                        // 3. Succès : Préparer la diffusion du nouvel état aux deux joueurs
                        char out_buf[MAX_MSG_SIZE];
                        PacketHeader* out_h = (PacketHeader*)out_buf;
                        GameStateUDP* gstate = (GameStateUDP*)(out_buf + sizeof(PacketHeader));

                        out_h->type = PACKET_GAME_STATE_UDP;
                        out_h->length = sizeof(PacketHeader) + sizeof(GameStateUDP);
                        gstate->game_id = move->game_id;
                        gstate->current_turn = (games[g_idx].state->current_player == PLAYER_WHITE) ? 0 : 1;
                        board_to_simple_string(games[g_idx].state, gstate->fen_board);

                        // Envoi de la mise à jour au joueur Blanc
                        out_h->client_id = games[g_idx].player_white;
                        ipc_msg_send(gateway_mq, out_buf, out_h->length, 1);

                        // Envoi de la mise à jour au joueur Noir
                        out_h->client_id = games[g_idx].player_black;
                        ipc_msg_send(gateway_mq, out_buf, out_h->length, 1);
                    } else {
                        // Échec : Le coup est illégal selon les règles des échecs
                        printf("[GameWorker] Game %d: Coup invalide de %d\n", move->game_id, header->client_id);
                        PacketHeader err_h = {PACKET_MOVE_ERROR, sizeof(PacketHeader), header->client_id};
                        ipc_msg_send(gateway_mq, &err_h, sizeof(err_h), 1);
                    }
                }
            }
        }
    }

    return 0;
}
