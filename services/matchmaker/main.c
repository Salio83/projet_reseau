#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "packet_types.h"
#include "ipc_utils.h"
#include "ipc_keys.h"

#define MAX_WAITING 100

typedef struct {
    uint32_t client_id;
    uint32_t player_id;
} WaitingPlayer;

WaitingPlayer waiting_players[MAX_WAITING];
int num_waiting = 0;

int main() {
    int mm_msqid = ipc_msg_get(GET_MATCHMAKING_KEY());
    int gateway_msqid = ipc_msg_get(GET_GATEWAY_KEY());

    printf("Matchmaker Service en ligne...\n");

    char buffer[MAX_MSG_SIZE];
    while (1) {
        int nbytes = ipc_msg_receive(mm_msqid, buffer, MAX_MSG_SIZE, 0);
        if (nbytes > 0) {
            PacketHeader *header = (PacketHeader *)buffer;
            MatchmakingRequest *req = (MatchmakingRequest *)(buffer + sizeof(PacketHeader));

            printf("Demande de matchmaking de joueur %d (client %d)\n", req->player_id, header->client_id);

            waiting_players[num_waiting].client_id = header->client_id;
            waiting_players[num_waiting].player_id = req->player_id;
            num_waiting++;

            if (num_waiting >= 2) {
                printf("Match trouvé entre %d et %d !\n", waiting_players[0].player_id, waiting_players[1].player_id);
                
                // On notifie les deux joueurs (via le gateway)
                // Pour l'instant on fait simple, on renvoie un message arbitraire
                
                header->type = 102; // Match Found
                header->client_id = waiting_players[0].client_id;
                ipc_msg_send(gateway_msqid, buffer, sizeof(PacketHeader), 1);
                
                header->client_id = waiting_players[1].client_id;
                ipc_msg_send(gateway_msqid, buffer, sizeof(PacketHeader), 1);

                num_waiting = 0;
            }
        }
    }

    return 0;
}
