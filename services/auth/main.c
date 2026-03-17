#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "packet_types.h"
#include "ipc_utils.h"
#include "ipc_keys.h"

int main() {
    int auth_msqid = ipc_msg_get(GET_AUTH_KEY());
    int gateway_msqid = ipc_msg_get(GET_GATEWAY_KEY());

    printf("Auth Service en ligne...\n");

    char buffer[MAX_MSG_SIZE];
    while (1) {
        int nbytes = ipc_msg_receive(auth_msqid, buffer, MAX_MSG_SIZE, 0);
        if (nbytes > 0) {
            PacketHeader *header = (PacketHeader *)buffer;
            AuthRequest *req = (AuthRequest *)(buffer + sizeof(PacketHeader));

            printf("Demande d'auth pour l'utilisateur: %s\n", req->username);

            // Simulation de validation (on accepte tout pour l'instant)
            // On peut renvoyer le même header mais avec peut-être un type de réponse
            // Pour l'instant on va juste logger
            
            // On renvoie une réponse au gateway
            // On pourrait définir PACKET_AUTH_RES dans packet_types.h
            // Mais pour l'instant on va juste renvoyer un message de succès
            
            header->type = 101; // Arbitraire pour Auth Success
            ipc_msg_send(gateway_msqid, buffer, header->length, 1);
        }
    }

    return 0;
}
