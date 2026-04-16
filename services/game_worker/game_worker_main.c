#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "../../common/chess_engine/chess.h"
#include "../../common/ipc_utils/ipc_keys.h"
#include "../../common/ipc_utils/ipc_utils.h"
#include "../../common/network_models/packet_types.h"

#define MAX_GAMES 50
#define MAX_SPECTATORS_PER_ROOM 32

typedef struct {
    uint32_t session_id;
    char username[MAX_USERNAME_LEN];
} SpectatorInfo;

typedef struct {
    uint32_t room_id;
    uint32_t player_white;
    uint32_t player_black;
    char white_name[MAX_USERNAME_LEN];
    char black_name[MAX_USERNAME_LEN];
    SpectatorInfo spectators[MAX_SPECTATORS_PER_ROOM];
    int spectator_count;
    GameState *state;
    uint32_t sequence_no;
    uint32_t tournament_id;
    int active;
} GameRoom;

static GameRoom rooms[MAX_GAMES];

static int find_room_index(uint32_t room_id) {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (rooms[i].active && rooms[i].room_id == room_id) {
            return i;
        }
    }
    return -1;
}

static GameRoom *find_room_for_session(uint32_t session_id) {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!rooms[i].active) {
            continue;
        }
        if (rooms[i].player_white == session_id || rooms[i].player_black == session_id) {
            return &rooms[i];
        }
        for (int j = 0; j < rooms[i].spectator_count; j++) {
            if (rooms[i].spectators[j].session_id == session_id) {
                return &rooms[i];
            }
        }
    }
    return NULL;
}

static void pos_to_coords(const char *pos, int *row, int *col) {
    *col = pos[0] - 'a';
    *row = 8 - (pos[1] - '0');
}

static void board_to_simple_string(GameState *state, char *buffer) {
    int pos = 0;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            Piece p = state->board[r][c];
            if (p.type == PIECE_NONE) {
                buffer[pos++] = '.';
            } else {
                char base = '?';
                switch (p.type) {
                    case PAWN: base = 'P'; break;
                    case KNIGHT: base = 'N'; break;
                    case BISHOP: base = 'B'; break;
                    case ROOK: base = 'R'; break;
                    case QUEEN: base = 'Q'; break;
                    case KING: base = 'K'; break;
                    default: break;
                }
                buffer[pos++] = (p.color == PLAYER_WHITE) ? base : (char)(base + ('a' - 'A'));
            }
        }
        buffer[pos++] = '/';
    }
    buffer[pos] = '\0';
}

static uint64_t now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);
}

static void send_to_gateway(uint32_t session_id, PacketType type, const void *payload, size_t payload_size) {
    int global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
    char out_buf[MAX_MSG_SIZE];
    PacketHeader *header = (PacketHeader *)out_buf;

    memset(out_buf, 0, sizeof(out_buf));
    header->type = (uint16_t)type;
    header->length = sizeof(PacketHeader) + (uint16_t)payload_size;
    header->session_id = session_id;

    if (payload && payload_size > 0) {
        memcpy(out_buf + sizeof(PacketHeader), payload, payload_size);
    }

    ipc_msg_send(global_mq, out_buf, header->length, MSG_TYPE_GATEWAY);
}

static void send_error(uint32_t session_id, uint16_t code, const char *message) {
    PacketError error;
    memset(&error, 0, sizeof(error));
    error.code = code;
    strncpy(error.message, message, sizeof(error.message) - 1);
    send_to_gateway(session_id, PACKET_ERROR, &error, sizeof(error));
}

static void send_move_error(uint32_t session_id) {
    send_to_gateway(session_id, PACKET_MOVE_ERROR, NULL, 0);
}

static void send_snapshot(uint32_t session_id, const GameRoom *room) {
    GameSnapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.room_id = room->room_id;
    snapshot.move_count = (uint16_t)room->state->move_count;
    snapshot.current_turn = (room->state->current_player == PLAYER_WHITE) ? 0 : 1;
    strncpy(snapshot.white_username, room->white_name, sizeof(snapshot.white_username) - 1);
    strncpy(snapshot.black_username, room->black_name, sizeof(snapshot.black_username) - 1);
    board_to_simple_string(room->state, snapshot.fen_board);
    send_to_gateway(session_id, PACKET_GAME_SNAPSHOT, &snapshot, sizeof(snapshot));
}

static void broadcast_update(const GameRoom *room, const PlayerMove *move) {
    GameUpdateUDP update;
    memset(&update, 0, sizeof(update));
    update.room_id = room->room_id;
    update.move_count = (uint16_t)room->state->move_count;
    update.current_turn = (room->state->current_player == PLAYER_WHITE) ? 0 : 1;
    update.sequence_no = room->sequence_no;
    strncpy(update.from_square, move->from_square, sizeof(update.from_square) - 1);
    strncpy(update.to_square, move->to_square, sizeof(update.to_square) - 1);
    update.promotion = move->promotion;
    board_to_simple_string(room->state, update.fen_board);

    send_to_gateway(room->player_white, PACKET_GAME_UPDATE_UDP, &update, sizeof(update));
    send_to_gateway(room->player_black, PACKET_GAME_UPDATE_UDP, &update, sizeof(update));

    for (int i = 0; i < room->spectator_count; i++) {
        send_to_gateway(room->spectators[i].session_id, PACKET_GAME_UPDATE_UDP, &update, sizeof(update));
    }
}

static const char *find_author_name(const GameRoom *room, uint32_t session_id) {
    if (room->player_white == session_id) {
        return room->white_name;
    }
    if (room->player_black == session_id) {
        return room->black_name;
    }
    for (int i = 0; i < room->spectator_count; i++) {
        if (room->spectators[i].session_id == session_id) {
            return room->spectators[i].username;
        }
    }
    return "unknown";
}

static void broadcast_chat(const GameRoom *room, uint32_t author_session_id, const char *message) {
    ChatBroadcast broadcast;
    memset(&broadcast, 0, sizeof(broadcast));
    broadcast.room_id = room->room_id;
    broadcast.author_session_id = author_session_id;
    broadcast.timestamp_ms = now_ms();
    strncpy(broadcast.author_name, find_author_name(room, author_session_id),
            sizeof(broadcast.author_name) - 1);
    strncpy(broadcast.message, message, sizeof(broadcast.message) - 1);

    send_to_gateway(room->player_white, PACKET_CHAT_BROADCAST, &broadcast, sizeof(broadcast));
    send_to_gateway(room->player_black, PACKET_CHAT_BROADCAST, &broadcast, sizeof(broadcast));
    for (int i = 0; i < room->spectator_count; i++) {
        send_to_gateway(room->spectators[i].session_id, PACKET_CHAT_BROADCAST, &broadcast,
                        sizeof(broadcast));
    }
}

static int room_contains_session(const GameRoom *room, uint32_t session_id) {
    if (room->player_white == session_id || room->player_black == session_id) {
        return 1;
    }
    for (int i = 0; i < room->spectator_count; i++) {
        if (room->spectators[i].session_id == session_id) {
            return 1;
        }
    }
    return 0;
}

static void remove_spectator(GameRoom *room, uint32_t session_id) {
    for (int i = 0; i < room->spectator_count; i++) {
        if (room->spectators[i].session_id == session_id) {
            for (int j = i; j < room->spectator_count - 1; j++) {
                room->spectators[j] = room->spectators[j + 1];
            }
            room->spectator_count--;
            return;
        }
    }
}

static void handle_room_creation(PacketHeader *header, GameStarted *started) {
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!rooms[i].active) {
            memset(&rooms[i], 0, sizeof(rooms[i]));
            rooms[i].active = 1;
            rooms[i].room_id = started->room_id;
            rooms[i].player_white = header->session_id;
            rooms[i].player_black = started->opponent_session_id;
            rooms[i].state = game_init();
            strncpy(rooms[i].white_name, started->white_username, sizeof(rooms[i].white_name) - 1);
            strncpy(rooms[i].black_name, started->black_username, sizeof(rooms[i].black_name) - 1);
            rooms[i].tournament_id = started->tournament_id;
            send_snapshot(rooms[i].player_white, &rooms[i]);
            send_snapshot(rooms[i].player_black, &rooms[i]);
            return;
        }
    }
}

static void handle_active_games_list(uint32_t session_id) {
    ActiveGamesResponse response;
    memset(&response, 0, sizeof(response));

    for (int i = 0; i < MAX_GAMES; i++) {
        if (!rooms[i].active) {
            continue;
        }
        if (response.game_count >= MAX_ACTIVE_GAMES_LISTED) {
            response.truncated = 1;
            break;
        }

        ActiveGameInfo *info = &response.games[response.game_count++];
        info->room_id = rooms[i].room_id;
        strncpy(info->white_username, rooms[i].white_name, sizeof(info->white_username) - 1);
        strncpy(info->black_username, rooms[i].black_name, sizeof(info->black_username) - 1);
        info->spectator_count = (uint16_t)rooms[i].spectator_count;
        info->move_count = (uint16_t)rooms[i].state->move_count;
        info->current_turn = (rooms[i].state->current_player == PLAYER_WHITE) ? 0 : 1;
        info->active = 1;
    }

    send_to_gateway(session_id, PACKET_LIST_ACTIVE_GAMES_RESP, &response, sizeof(response));
}

static void handle_spectate_join(uint32_t session_id, SpectateJoinRequest *request) {
    int index = find_room_index(request->room_id);
    if (index == -1) {
        send_error(session_id, 404, "Salon introuvable.");
        return;
    }

    GameRoom *room = &rooms[index];
    if (room->player_white == session_id || room->player_black == session_id) {
        send_error(session_id, 409, "Vous jouez déjà dans ce salon.");
        return;
    }

    for (int i = 0; i < room->spectator_count; i++) {
        if (room->spectators[i].session_id == session_id) {
            SpectateStatus status = {room->room_id};
            send_to_gateway(session_id, PACKET_SPECTATE_JOIN_OK, &status, sizeof(status));
            send_snapshot(session_id, room);
            return;
        }
    }

    if (room->spectator_count >= MAX_SPECTATORS_PER_ROOM) {
        send_error(session_id, 409, "Salon plein côté spectateurs.");
        return;
    }

    room->spectators[room->spectator_count].session_id = session_id;
    strncpy(room->spectators[room->spectator_count].username, request->username,
            sizeof(room->spectators[room->spectator_count].username) - 1);
    room->spectator_count++;

    SpectateStatus status = {room->room_id};
    send_to_gateway(session_id, PACKET_SPECTATE_JOIN_OK, &status, sizeof(status));
    send_snapshot(session_id, room);
}

static void handle_spectate_leave(uint32_t session_id) {
    GameRoom *room = find_room_for_session(session_id);
    SpectateStatus status;

    if (!room) {
        send_error(session_id, 404, "Aucun salon à quitter.");
        return;
    }

    remove_spectator(room, session_id);
    status.room_id = room->room_id;
    send_to_gateway(session_id, PACKET_SPECTATE_LEAVE_OK, &status, sizeof(status));
}

static void handle_move(uint32_t session_id, PlayerMove *move) {
    int index = find_room_index(move->game_id);
    if (index == -1) {
        send_move_error(session_id);
        return;
    }

    GameRoom *room = &rooms[index];
    uint32_t expected_player =
        (room->state->current_player == PLAYER_WHITE) ? room->player_white : room->player_black;

    if (session_id != expected_player) {
        send_move_error(session_id);
        return;
    }

    int from_row, from_col, to_row, to_col;
    pos_to_coords(move->from_square, &from_row, &from_col);
    pos_to_coords(move->to_square, &to_row, &to_col);

    if (!game_make_move(room->state, from_row, from_col, to_row, to_col)) {
        send_move_error(session_id);
        return;
    }

    room->sequence_no++;
    broadcast_update(room, move);

    // Vérifier la fin de partie
    if (room->state->checkmate || room->state->stalemate) {
        int global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
        
        // Envoi pour l'historique et les statistiques
        char hist_buf[MAX_MSG_SIZE];
        PacketHeader *hist_header = (PacketHeader *)hist_buf;
        SaveHistoryReq *hist_req = (SaveHistoryReq *)(hist_buf + sizeof(PacketHeader));
        memset(hist_buf, 0, sizeof(hist_buf));

        hist_header->type = PACKET_SAVE_HISTORY;
        hist_header->length = sizeof(PacketHeader) + sizeof(SaveHistoryReq);
        hist_req->room_id = room->room_id;
        strncpy(hist_req->white_name, room->white_name, sizeof(hist_req->white_name) - 1);
        strncpy(hist_req->black_name, room->black_name, sizeof(hist_req->black_name) - 1);
        hist_req->move_count = room->state->move_count;
        if (room->state->checkmate) {
            hist_req->result = (room->state->current_player == PLAYER_WHITE) ? 2 : 1; // if white is checked, black (2) wins
        } else {
            hist_req->result = 0; // draw
        }
        ipc_msg_send(global_mq, hist_buf, hist_header->length, MSG_TYPE_STORAGE);

        // Si tournoi, on notifie le Tournoi
        if (room->tournament_id != 0) {
            char out_buf[MAX_MSG_SIZE];
            PacketHeader *out_header = (PacketHeader *)out_buf;
            GameFinished *finished = (GameFinished *)(out_buf + sizeof(PacketHeader));
            memset(out_buf, 0, sizeof(out_buf));

            out_header->type = PACKET_GAME_FINISHED;
            out_header->length = sizeof(PacketHeader) + sizeof(GameFinished);
            
            finished->tournament_id = room->tournament_id;
            
            if (room->state->checkmate) {
                // Le joueur actuel vient de jouer et de mettre échec et mat, attention la logique checkmate c'est current_player maté.
                // Donc si current_player == white, black a gagné.  Mais dans `handle_move`, on a déjà joué le coup.
                // `session_id` est le joueur qui a FAIT le coup.
                finished->winner_session_id = session_id;
            } else {
                // Pat : arbitrairement on qualifie les Noirs (ou autre décision)
                finished->winner_session_id = room->player_black;
            }
            
            ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_TOURNAMENT);
        }
        room->active = 0; // Fermer la salle
    }
}

static void handle_chat(uint32_t session_id, ChatMessage *message) {
    int index = find_room_index(message->room_id);
    if (index == -1) {
        send_error(session_id, 404, "Salon introuvable.");
        return;
    }

    GameRoom *room = &rooms[index];
    if (!room_contains_session(room, session_id)) {
        send_error(session_id, 403, "Non autorisé dans ce salon.");
        return;
    }
    if (message->message[0] == '\0') {
        send_error(session_id, 400, "Message vide.");
        return;
    }

    broadcast_chat(room, session_id, message->message);
}

static void handle_disconnect(PacketHeader *header, ClientDisconnected *disconnected) {
    (void)disconnected;
    GameRoom *room = find_room_for_session(header->session_id);
    if (!room) {
        return;
    }
    remove_spectator(room, header->session_id);
    
    // Si c'est un joueur qui déconnecte en plein tournoi
    if (room->tournament_id != 0 && (room->player_white == header->session_id || room->player_black == header->session_id)) {
        char out_buf[MAX_MSG_SIZE];
        PacketHeader *out_header = (PacketHeader *)out_buf;
        GameFinished *finished = (GameFinished *)(out_buf + sizeof(PacketHeader));
        memset(out_buf, 0, sizeof(out_buf));

        out_header->type = PACKET_GAME_FINISHED;
        out_header->length = sizeof(PacketHeader) + sizeof(GameFinished);
        finished->tournament_id = room->tournament_id;
        
        // Le gagnant est l'autre joueur
        if (room->player_white == header->session_id) {
            finished->winner_session_id = room->player_black;
        } else {
            finished->winner_session_id = room->player_white;
        }
        
        int global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
        ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_TOURNAMENT);
        
        room->active = 0;
    }
}

int main(void) {
    printf("[GameWorker] Démarrage...\n");

    int global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
    if (global_mq == -1) {
        perror("ipc_msg_get failed");
        exit(1);
    }

    memset(rooms, 0, sizeof(rooms));

    char msg_buffer[MAX_MSG_SIZE];
    while (1) {
        int nbytes = ipc_msg_receive(global_mq, msg_buffer, sizeof(msg_buffer), MSG_TYPE_GAMEWORKER);
        if (nbytes <= 0) {
            continue;
        }

        PacketHeader *header = (PacketHeader *)msg_buffer;
        void *payload = msg_buffer + sizeof(PacketHeader);

        switch (header->type) {
            case PACKET_GAME_STARTED:
                handle_room_creation(header, (GameStarted *)payload);
                break;
            case PACKET_LIST_ACTIVE_GAMES_REQ:
                handle_active_games_list(header->session_id);
                break;
            case PACKET_SPECTATE_JOIN_REQ:
                handle_spectate_join(header->session_id, (SpectateJoinRequest *)payload);
                break;
            case PACKET_SPECTATE_LEAVE_REQ:
                handle_spectate_leave(header->session_id);
                break;
            case PACKET_PLAYER_MOVE:
                handle_move(header->session_id, (PlayerMove *)payload);
                break;
            case PACKET_CHAT_MSG:
                handle_chat(header->session_id, (ChatMessage *)payload);
                break;
            case PACKET_CLIENT_DISCONNECTED:
                handle_disconnect(header, (ClientDisconnected *)payload);
                break;
            default:
                break;
        }
    }

    return 0;
}
