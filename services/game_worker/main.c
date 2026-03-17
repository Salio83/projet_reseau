#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "packet_types.h"
#include "ipc_utils.h"
#include "ipc_keys.h"
#include "chess.h"

int main() {
    int game_msqid = ipc_msg_get(GET_GAME_WORKER_KEY());
    int gateway_msqid = ipc_msg_get(GET_GATEWAY_KEY());

    printf("Game Worker Service en ligne...\n");

    GameState *game = game_init();

    char buffer[MAX_MSG_SIZE];
    while (1) {
        int nbytes = ipc_msg_receive(game_msqid, buffer, MAX_MSG_SIZE, 0);
        if (nbytes > 0) {
            PacketHeader *header = (PacketHeader *)buffer;
            PlayerMove *move = (PlayerMove *)(buffer + sizeof(PacketHeader));

            printf("Coup reçu du client %d: de %s vers %s\n", header->client_id, move->from_square, move->to_square);

            // Conversion e2e4 -> (6,4) -> (4,4)
            int from_col = move->from_square[0] - 'a';
            int from_row = 8 - (move->from_square[1] - '0');
            int to_col = move->to_square[0] - 'a';
            int to_row = 8 - (move->to_square[1] - '0');

            if (game_make_move(game, from_row, from_col, to_row, to_col)) {
                printf("Coup valide ! Nouveau tour : %s\n", (game->current_player == PLAYER_WHITE) ? "Blanc" : "Noir");
                
                // Envoi d'un accusé de réception
                header->type = 103; // Move Ack
                ipc_msg_send(gateway_msqid, buffer, header->length, 1);
            } else {
                printf("Coup invalide !\n");
                // TODO: Envoyer erreur au client
            }
        }
    }

    game_free(game);
    return 0;
}
