#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../common/ipc_utils/ipc_keys.h"
#include "../../common/ipc_utils/ipc_utils.h"
#include "../../common/network_models/packet_types.h"

#define MAX_WAITING 100

typedef struct {
    uint32_t session_id;
    char username[MAX_USERNAME_LEN];
} WaitingPlayer;

int main(void) {
    printf("[Matchmaker] Démarrage...\n");

    int global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
    if (global_mq == -1) {
        perror("ipc_msg_get failed");
        exit(1);
    }

    WaitingPlayer waiting_players[MAX_WAITING];
    int waiting_count = 0;
    uint32_t next_room_id = 1;
    char msg_buffer[MAX_MSG_SIZE];

    while (1) {
        int nbytes = ipc_msg_receive(global_mq, msg_buffer, sizeof(msg_buffer), MSG_TYPE_MATCHMAKING);
        if (nbytes <= 0) {
            continue;
        }

        PacketHeader *header = (PacketHeader *)msg_buffer;
        MatchmakingRequest *request = (MatchmakingRequest *)(msg_buffer + sizeof(PacketHeader));

        int duplicate = 0;
        for (int i = 0; i < waiting_count; i++) {
            if (waiting_players[i].session_id == header->session_id) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate || waiting_count >= MAX_WAITING) {
            continue;
        }

        waiting_players[waiting_count].session_id = header->session_id;
        strncpy(waiting_players[waiting_count].username, request->username,
                sizeof(waiting_players[waiting_count].username) - 1);
        waiting_count++;

        if (waiting_count < 2) {
            continue;
        }

        WaitingPlayer white = waiting_players[0];
        WaitingPlayer black = waiting_players[1];
        uint32_t room_id = next_room_id++;

        char out_buf[MAX_MSG_SIZE];
        PacketHeader *out_header = (PacketHeader *)out_buf;
        GameStarted *started = (GameStarted *)(out_buf + sizeof(PacketHeader));

        memset(out_buf, 0, sizeof(out_buf));
        out_header->type = PACKET_GAME_STARTED;
        out_header->length = sizeof(PacketHeader) + sizeof(GameStarted);

        started->room_id = room_id;
        strncpy(started->white_username, white.username, sizeof(started->white_username) - 1);
        strncpy(started->black_username, black.username, sizeof(started->black_username) - 1);

        out_header->session_id = white.session_id;
        started->opponent_session_id = black.session_id;
        started->your_color = 0;
        strncpy(started->opponent_username, black.username, sizeof(started->opponent_username) - 1);
        ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GATEWAY);

        out_header->session_id = black.session_id;
        started->opponent_session_id = white.session_id;
        started->your_color = 1;
        strncpy(started->opponent_username, white.username, sizeof(started->opponent_username) - 1);
        ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GATEWAY);

        out_header->session_id = white.session_id;
        started->opponent_session_id = black.session_id;
        started->your_color = 0;
        strncpy(started->opponent_username, black.username, sizeof(started->opponent_username) - 1);
        ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GAMEWORKER);

        waiting_count -= 2;
        for (int i = 0; i < waiting_count; i++) {
            waiting_players[i] = waiting_players[i + 2];
        }
    }

    return 0;
}
