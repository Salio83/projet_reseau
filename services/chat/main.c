#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "packet_types.h"
#include "ipc_utils.h"
#include "ipc_keys.h"

int main() {
    int chat_msqid = ipc_msg_get(GET_CHAT_KEY());
    int gateway_msqid = ipc_msg_get(GET_GATEWAY_KEY());

    printf("Chat Service en ligne...\n");

    char buffer[MAX_MSG_SIZE];
    while (1) {
        int nbytes = ipc_msg_receive(chat_msqid, buffer, MAX_MSG_SIZE, 0);
        if (nbytes > 0) {
            PacketHeader *header = (PacketHeader *)buffer;
            ChatMessage *req = (ChatMessage *)(buffer + sizeof(PacketHeader));

            printf("Chat Room %d: Client %d dit '%s'\n", req->room_id, header->client_id, req->message);

            // TODO: Broadcast à tous les clients de la salle
            // On renvoie juste un accusé de réception pour l'instant
            
            header->type = 104; // Chat Ack
            ipc_msg_send(gateway_msqid, buffer, header->length, 1);
        }
    }

    return 0;
}
