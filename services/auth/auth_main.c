#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../common/ipc_utils/ipc_keys.h"
#include "../../common/ipc_utils/ipc_utils.h"
#include "../../common/network_models/packet_types.h"

int main(void) {
    printf("[Auth Service] Démarrage...\n");

    int fd = open(GLOBAL_MSG_QUEUE_PATH, O_CREAT | O_RDWR, 0666);
    if (fd != -1) {
        close(fd);
    }

    int global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
    if (global_mq == -1) {
        perror("ipc_msg_get failed");
        exit(1);
    }

    char msg_buffer[MAX_MSG_SIZE];

    while (1) {
        int nbytes = ipc_msg_receive(global_mq, msg_buffer, sizeof(msg_buffer), MSG_TYPE_AUTH);
        if (nbytes <= 0) {
            continue;
        }

        PacketHeader *header = (PacketHeader *)msg_buffer;
        AuthRequest *request = (AuthRequest *)(msg_buffer + sizeof(PacketHeader));

        char out_buf[MAX_MSG_SIZE];
        PacketHeader *out_header = (PacketHeader *)out_buf;
        AuthOk *response = (AuthOk *)(out_buf + sizeof(PacketHeader));

        memset(out_buf, 0, sizeof(out_buf));
        out_header->type = PACKET_AUTH_OK;
        out_header->length = sizeof(PacketHeader) + sizeof(AuthOk);
        out_header->session_id = header->session_id;

        response->session_id = header->session_id;
        strncpy(response->username, request->username, sizeof(response->username) - 1);

        ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GATEWAY);
    }

    return 0;
}
