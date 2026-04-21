#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "../../common/ipc_utils/ipc_keys.h"
#include "../../common/ipc_utils/ipc_utils.h"
#include "../../common/network_models/packet_types.h"

#define MAX_SESSIONS 256

typedef struct {
    uint32_t session_id;
    uint32_t room_id;
    char username[MAX_USERNAME_LEN];
    int active;
} ChatSession;

static ChatSession sessions[MAX_SESSIONS];

static void register_session(uint32_t session_id, uint32_t room_id, const char *username) {
    int first_free = -1;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active && sessions[i].session_id == session_id) {
            sessions[i].room_id = room_id;
            if (username && strlen(username) > 0) {
                strncpy(sessions[i].username, username, MAX_USERNAME_LEN - 1);
            }
            return;
        }
        if (!sessions[i].active && first_free == -1) {
            first_free = i;
        }
    }

    if (first_free != -1) {
        sessions[first_free].active = 1;
        sessions[first_free].session_id = session_id;
        sessions[first_free].room_id = room_id;
        if (username && strlen(username) > 0) {
            strncpy(sessions[first_free].username, username, MAX_USERNAME_LEN - 1);
        } else {
            snprintf(sessions[first_free].username, MAX_USERNAME_LEN, "User%u", session_id);
        }
    }
}

static void unregister_session(uint32_t session_id) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active && sessions[i].session_id == session_id) {
            sessions[i].active = 0;
            return;
        }
    }
}

static ChatSession *find_session(uint32_t session_id) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active && sessions[i].session_id == session_id) {
            return &sessions[i];
        }
    }
    return NULL;
}

static void broadcast_to_room(int mq, uint32_t room_id, const char *author_name, uint32_t author_session_id, const char *message) {
    char out_buf[MAX_MSG_SIZE];
    PacketHeader *header = (PacketHeader *)out_buf;
    ChatBroadcast *broadcast = (ChatBroadcast *)(out_buf + sizeof(PacketHeader));

    memset(out_buf, 0, sizeof(out_buf));
    header->type = PACKET_CHAT_BROADCAST;
    header->length = sizeof(PacketHeader) + sizeof(ChatBroadcast);

    broadcast->room_id = room_id;
    strncpy(broadcast->author_name, author_name, MAX_USERNAME_LEN - 1);
    broadcast->author_session_id = author_session_id;
    broadcast->timestamp_ms = (uint64_t)time(NULL) * 1000;
    strncpy(broadcast->message, message, MAX_CHAT_MESSAGE_LEN - 1);

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active && sessions[i].room_id == room_id) {
            header->session_id = sessions[i].session_id;
            ipc_msg_send(mq, out_buf, header->length, MSG_TYPE_GATEWAY);
        }
    }
}

int main(void) {
    printf("[Chat Service] Démarrage...\n");

    int global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
    if (global_mq == -1) {
        perror("[Chat Service] ipc_msg_get failed");
        exit(1);
    }

    memset(sessions, 0, sizeof(sessions));

    char msg_buffer[MAX_MSG_SIZE];
    while (1) {
        int nbytes = ipc_msg_receive(global_mq, msg_buffer, sizeof(msg_buffer), MSG_TYPE_CHAT);
        if (nbytes <= 0) {
            if (nbytes < 0 && (errno == EIDRM || errno == EINVAL)) {
                break;
            }
            continue;
        }

        PacketHeader *header = (PacketHeader *)msg_buffer;
        void *payload = msg_buffer + sizeof(PacketHeader);

        switch (header->type) {
            case PACKET_AUTH_OK: {
                AuthOk *ok = (AuthOk *)payload;
                register_session(header->session_id, 0, ok->username);
                break;
            }
            case PACKET_GAME_STARTED: {
                GameStarted *gs = (GameStarted *)payload;
                const char *name = (gs->your_color == 0) ? gs->white_username : gs->black_username;
                register_session(header->session_id, gs->room_id, name);
                break;
            }
            case PACKET_SPECTATE_JOIN_OK: {
                SpectateStatus *ss = (SpectateStatus *)payload;
                register_session(header->session_id, ss->room_id, NULL);
                break;
            }
            case PACKET_SPECTATE_LEAVE_OK:
            case PACKET_CLIENT_DISCONNECTED: {
                unregister_session(header->session_id);
                break;
            }
            case PACKET_CHAT_MSG: {
                ChatMessage *cm = (ChatMessage *)payload;
                ChatSession *s = find_session(header->session_id);
                if (s) {
                    broadcast_to_room(global_mq, cm->room_id, s->username, s->session_id, cm->message);
                } else {
                    broadcast_to_room(global_mq, cm->room_id, "Unknown", header->session_id, cm->message);
                }
                break;
            }
            default:
                break;
        }
    }

    return 0;
}
