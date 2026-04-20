#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "../../common/ipc_utils/ipc_keys.h"
#include "../../common/ipc_utils/ipc_utils.h"
#include "../../common/network_models/packet_types.h"

#define MAX_ROOMS 100
#define MAX_SPECTATORS_PER_ROOM 10

typedef struct {
    uint32_t session_id;
    char username[MAX_USERNAME_LEN];
} Participant;

typedef struct {
    uint32_t room_id;
    uint32_t player_white;
    uint32_t player_black;
    char white_name[MAX_USERNAME_LEN];
    char black_name[MAX_USERNAME_LEN];
    Participant spectators[MAX_SPECTATORS_PER_ROOM];
    int spectator_count;
    int active;
} ChatRoom;

static ChatRoom rooms[MAX_ROOMS];
static int global_mq = -1;

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int find_room_index(uint32_t room_id) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active && rooms[i].room_id == room_id) {
            return i;
        }
    }
    return -1;
}

// Improved version: use PacketHeader to know who is who
static void update_room_member(PacketHeader *header, void *payload) {
    uint32_t room_id = 0;
    if (header->type == PACKET_GAME_STARTED) {
        GameStarted *gs = (GameStarted *)payload;
        room_id = gs->room_id;
        int idx = find_room_index(room_id);
        if (idx == -1) {
            for (int i = 0; i < MAX_ROOMS; i++) {
                if (!rooms[i].active) { idx = i; break; }
            }
        }
        if (idx != -1) {
            rooms[idx].active = 1;
            rooms[idx].room_id = room_id;
            if (gs->your_color == 0) {
                rooms[idx].player_white = header->session_id;
                strncpy(rooms[idx].white_name, "White", MAX_USERNAME_LEN-1); // Name not in GameStarted for self
                rooms[idx].player_black = gs->opponent_session_id;
                strncpy(rooms[idx].black_name, gs->opponent_username, MAX_USERNAME_LEN-1);
            } else {
                rooms[idx].player_black = header->session_id;
                strncpy(rooms[idx].black_name, "Black", MAX_USERNAME_LEN-1);
                rooms[idx].player_white = gs->opponent_session_id;
                strncpy(rooms[idx].white_name, gs->opponent_username, MAX_USERNAME_LEN-1);
            }
        }
    } else if (header->type == PACKET_SPECTATE_JOIN_OK) {
        SpectateStatus *ss = (SpectateStatus *)payload;
        int idx = find_room_index(ss->room_id);
        if (idx != -1) {
            if (rooms[idx].spectator_count < MAX_SPECTATORS_PER_ROOM) {
                rooms[idx].spectators[rooms[idx].spectator_count].session_id = header->session_id;
                snprintf(rooms[idx].spectators[rooms[idx].spectator_count].username, MAX_USERNAME_LEN, "Spec_%u", header->session_id);
                rooms[idx].spectator_count++;
            }
        }
    }
}

static void remove_session(uint32_t session_id) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].active) continue;
        if (rooms[i].player_white == session_id || rooms[i].player_black == session_id) {
            // Room closed if player leaves? For chat, let's just mark inactive if both gone or similar.
            // Simplified: if a player leaves, we might as well keep it for remaining spectators, 
            // but usually room closes.
        }
        for (int j = 0; j < rooms[i].spectator_count; j++) {
            if (rooms[i].spectators[j].session_id == session_id) {
                rooms[i].spectators[j] = rooms[i].spectators[rooms[i].spectator_count - 1];
                rooms[i].spectator_count--;
                break;
            }
        }
    }
}

static const char* find_name(ChatRoom *room, uint32_t session_id) {
    if (session_id == room->player_white) return room->white_name;
    if (session_id == room->player_black) return room->black_name;
    for (int i = 0; i < room->spectator_count; i++) {
        if (room->spectators[i].session_id == session_id) return room->spectators[i].username;
    }
    return "Unknown";
}

static void handle_chat_msg(PacketHeader *header, ChatMessage *msg) {
    int idx = find_room_index(msg->room_id);
    if (idx == -1) return;

    ChatRoom *room = &rooms[idx];
    
    ChatBroadcast bc;
    memset(&bc, 0, sizeof(bc));
    bc.room_id = room->room_id;
    bc.author_session_id = header->session_id;
    bc.timestamp_ms = now_ms();
    strncpy(bc.author_name, find_name(room, header->session_id), MAX_USERNAME_LEN - 1);
    strncpy(bc.message, msg->message, MAX_CHAT_MESSAGE_LEN - 1);

    // Broadcast to all participants
    if (room->player_white != 0) ipc_msg_send(global_mq, &bc, sizeof(bc), MSG_TYPE_GATEWAY);
    // Note: session_id in header for broadcast needs to be target
    
    void send_to(uint32_t target_sid, ChatBroadcast *b) {
        char buf[MAX_MSG_SIZE];
        PacketHeader *h = (PacketHeader *)buf;
        h->type = PACKET_CHAT_BROADCAST;
        h->length = sizeof(PacketHeader) + sizeof(ChatBroadcast);
        h->session_id = target_sid;
        memcpy(buf + sizeof(PacketHeader), b, sizeof(ChatBroadcast));
        ipc_msg_send(global_mq, buf, h->length, MSG_TYPE_GATEWAY);
    }

    if (room->player_white) send_to(room->player_white, &bc);
    if (room->player_black) send_to(room->player_black, &bc);
    for (int i = 0; i < room->spectator_count; i++) {
        send_to(room->spectators[i].session_id, &bc);
    }
    
    printf("[Chat] Broadcast in room %u from %s: %s\n", room->room_id, bc.author_name, bc.message);
}

int main(void) {
    printf("[Chat Service] Démarrage...\n");

    global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
    if (global_mq == -1) {
        perror("ipc_msg_get");
        exit(1);
    }

    memset(rooms, 0, sizeof(rooms));

    char buf[MAX_MSG_SIZE];
    while (1) {
        int n = ipc_msg_receive(global_mq, buf, sizeof(buf), MSG_TYPE_CHAT);
        if (n <= 0) continue;

        PacketHeader *header = (PacketHeader *)buf;
        void *payload = buf + sizeof(PacketHeader);

        switch (header->type) {
            case PACKET_GAME_STARTED:
            case PACKET_SPECTATE_JOIN_OK:
                update_room_member(header, payload);
                break;
            case PACKET_SPECTATE_LEAVE_OK:
            case PACKET_CLIENT_DISCONNECTED:
                remove_session(header->session_id);
                break;
            case PACKET_CHAT_MSG:
                handle_chat_msg(header, (ChatMessage *)payload);
                break;
        }
    }
    return 0;
}
