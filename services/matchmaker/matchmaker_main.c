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

static WaitingPlayer waiting_players[MAX_WAITING];
static int waiting_count = 0;
static int global_mq = -1;

static void broadcast_queue_status(void) {
    char out_buf[MAX_MSG_SIZE];
    PacketHeader *out_header = (PacketHeader *)out_buf;
    MatchmakingStatus *status = (MatchmakingStatus *)(out_buf + sizeof(PacketHeader));

    // 1. Broadcast global du nombre de joueurs en attente
    memset(out_buf, 0, sizeof(out_buf));
    out_header->type = PACKET_MATCHMAKING_STATUS;
    out_header->length = sizeof(PacketHeader) + sizeof(MatchmakingStatus);
    out_header->session_id = 0xFFFFFFFF; // Special broadcast ID
    status->queue_size = waiting_count;
    status->position = 0; // 0 signifie "non en attente" ou info générale
    ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GATEWAY);

    // 2. Notification individuelle de la position pour ceux qui attendent
    for (int i = 0; i < waiting_count; i++) {
        memset(out_buf, 0, sizeof(out_buf));
        out_header->type = PACKET_MATCHMAKING_STATUS;
        out_header->length = sizeof(PacketHeader) + sizeof(MatchmakingStatus);
        out_header->session_id = waiting_players[i].session_id;
        status->queue_size = waiting_count;
        status->position = i + 1;
        ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GATEWAY);
    }
}

static void handle_matchmaking_req(PacketHeader *header, MatchmakingRequest *request) {
    int duplicate = -1;
    for (int i = 0; i < waiting_count; i++) {
        if (waiting_players[i].session_id == header->session_id) {
            duplicate = i;
            break;
        }
    }

    if (duplicate != -1) return;
    if (waiting_count >= MAX_WAITING) return;

    waiting_players[waiting_count].session_id = header->session_id;
    strncpy(waiting_players[waiting_count].username, request->username, sizeof(waiting_players[waiting_count].username) - 1);
    waiting_count++;

    printf("[Matchmaker] Player %s joined the queue. Queue size: %d\n", request->username, waiting_count);
    broadcast_queue_status();
}

static void handle_disconnect(uint32_t session_id) {
    int found = -1;
    for (int i = 0; i < waiting_count; i++) {
        if (waiting_players[i].session_id == session_id) {
            found = i;
            break;
        }
    }

    if (found != -1) {
        printf("[Matchmaker] Player %s left the queue due to disconnect.\n", waiting_players[found].username);
        for (int i = found; i < waiting_count - 1; i++) {
            waiting_players[i] = waiting_players[i + 1];
        }
        waiting_count--;
        broadcast_queue_status();
    }
}

static void try_match(uint32_t *next_room_id) {
    if (waiting_count < 2) return;

    WaitingPlayer white = waiting_players[0];
    WaitingPlayer black = waiting_players[1];
    uint32_t room_id = (*next_room_id)++;

    // Supprimer les deux joueurs de la file
    for (int i = 0; i < waiting_count - 2; i++) {
        waiting_players[i] = waiting_players[i + 2];
    }
    waiting_count -= 2;

    char out_buf[MAX_MSG_SIZE];
    PacketHeader *out_header = (PacketHeader *)out_buf;
    GameStarted *started = (GameStarted *)(out_buf + sizeof(PacketHeader));

    memset(out_buf, 0, sizeof(out_buf));
    out_header->type = PACKET_GAME_STARTED;
    out_header->length = sizeof(PacketHeader) + sizeof(GameStarted);
    started->room_id = room_id;
    strncpy(started->white_username, white.username, sizeof(started->white_username) - 1);
    strncpy(started->black_username, black.username, sizeof(started->black_username) - 1);

    // Send to White
    out_header->session_id = white.session_id;
    started->opponent_session_id = black.session_id;
    started->your_color = 0;
    strncpy(started->opponent_username, black.username, sizeof(started->opponent_username) - 1);
    ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GATEWAY);

    // Send to Black
    out_header->session_id = black.session_id;
    started->opponent_session_id = white.session_id;
    started->your_color = 1;
    strncpy(started->opponent_username, white.username, sizeof(started->opponent_username) - 1);
    ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GATEWAY);

    // Notify GameWorker
    out_header->session_id = white.session_id;
    started->opponent_session_id = black.session_id;
    started->your_color = 0;
    strncpy(started->opponent_username, black.username, sizeof(started->opponent_username) - 1);
    ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GAMEWORKER);

    printf("[Matchmaker] Match created: %s vs %s in room %u\n", white.username, black.username, room_id);
    broadcast_queue_status();
}

int main(void) {
    printf("[Matchmaker] Démarrage...\n");

    global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
    if (global_mq == -1) {
        perror("ipc_msg_get failed");
        exit(1);
    }

    uint32_t next_room_id = 1;
    char msg_buffer[MAX_MSG_SIZE];

    while (1) {
        int nbytes = ipc_msg_receive(global_mq, msg_buffer, sizeof(msg_buffer), MSG_TYPE_MATCHMAKING);
        if (nbytes <= 0) continue;

        PacketHeader *header = (PacketHeader *)msg_buffer;
        void *payload = msg_buffer + sizeof(PacketHeader);

        switch (header->type) {
            case PACKET_MATCHMAKING_REQ:
                handle_matchmaking_req(header, (MatchmakingRequest *)payload);
                try_match(&next_room_id);
                break;
            case PACKET_CLIENT_DISCONNECTED:
                handle_disconnect(header->session_id);
                break;
            default:
                break;
        }
    }

    return 0;
}
