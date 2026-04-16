#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../common/ipc_utils/ipc_keys.h"
#include "../../common/ipc_utils/ipc_utils.h"
#include "../../common/network_models/packet_types.h"

#define MAX_TOURNAMENTS 10
#define MAX_T_PLAYERS 32
#define MAX_MATCHES (MAX_T_PLAYERS * 2)

typedef struct {
    uint32_t session_id;
    char username[MAX_USERNAME_LEN];
} TPlayer;

typedef struct {
    int valid;
    uint32_t p1;
    uint32_t p2;
    char p1_name[MAX_USERNAME_LEN];
    char p2_name[MAX_USERNAME_LEN];
    uint32_t winner;
    int next_match_idx; // -1 if final
    int started;
} TMatch;

typedef struct {
    uint32_t id;
    int active; // 0=none, 1=waiting, 2=running
    int max_players;
    TPlayer players[MAX_T_PLAYERS];
    int player_count;
    TMatch matches[MAX_MATCHES];
    int num_matches;
} Tournament;

static Tournament tournaments[MAX_TOURNAMENTS];
static uint32_t next_tourney_id = 1;
static uint32_t next_room_id = 10000; // start high to avoid collision with matchmaking
static int global_mq = -1;

static int next_power_of_2(int n) {
    int p = 1;
    while (p < n) p *= 2;
    return p;
}

static void send_to_gateway(uint32_t session_id, PacketType type, const void *payload, size_t payload_size) {
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

static void send_start_room(Tournament *t, int match_index) {
    TMatch *m = &t->matches[match_index];
    if (m->started || m->winner != 0 || m->p1 == 0 || m->p2 == 0) return;

    m->started = 1;
    uint32_t room_id = next_room_id++;

    char out_buf[MAX_MSG_SIZE];
    PacketHeader *out_header = (PacketHeader *)out_buf;
    GameStarted *started = (GameStarted *)(out_buf + sizeof(PacketHeader));

    memset(out_buf, 0, sizeof(out_buf));
    out_header->type = PACKET_GAME_STARTED;
    out_header->length = sizeof(PacketHeader) + sizeof(GameStarted);

    started->room_id = room_id;
    started->tournament_id = t->id;
    strncpy(started->white_username, m->p1_name, sizeof(started->white_username) - 1);
    strncpy(started->black_username, m->p2_name, sizeof(started->black_username) - 1);

    // Send to Player 1 (White)
    out_header->session_id = m->p1;
    started->opponent_session_id = m->p2;
    started->your_color = 0;
    strncpy(started->opponent_username, m->p2_name, sizeof(started->opponent_username) - 1);
    ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GATEWAY);

    // Send to Player 2 (Black)
    out_header->session_id = m->p2;
    started->opponent_session_id = m->p1;
    started->your_color = 1;
    strncpy(started->opponent_username, m->p1_name, sizeof(started->opponent_username) - 1);
    ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GATEWAY);

    // Send to Gameworker
    out_header->session_id = m->p1;
    started->opponent_session_id = m->p2;
    started->your_color = 0;
    strncpy(started->opponent_username, m->p2_name, sizeof(started->opponent_username) - 1);
    ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GAMEWORKER);
    
    printf("[Tournament] Match started in room %d for Tournament %d\n", room_id, t->id);
}

static void advance_winner(Tournament *t, int match_index, uint32_t winner_id, const char* winner_name) {
    TMatch *m = &t->matches[match_index];
    m->winner = winner_id;
    
    if (m->next_match_idx == -1) {
        printf("[Tournament] Tournament %d finished! Winner: %s\n", t->id, winner_name);
        t->active = 0; // Terminé
        return;
    }

    TMatch *next_m = &t->matches[m->next_match_idx];
    if (next_m->p1 == 0) {
        next_m->p1 = winner_id;
        strncpy(next_m->p1_name, winner_name, MAX_USERNAME_LEN - 1);
    } else {
        next_m->p2 = winner_id;
        strncpy(next_m->p2_name, winner_name, MAX_USERNAME_LEN - 1);
    }

    // Si le prochain match est prêt, le lancer
    if (next_m->p1 != 0 && next_m->p2 != 0) {
        send_start_room(t, m->next_match_idx);
    }
}

static void build_bracket(Tournament *t) {
    int p = next_power_of_2(t->player_count);
    int byes = p - t->player_count;
    int num_matches = p - 1;
    t->num_matches = num_matches;

    for (int i = 0; i < num_matches; i++) {
        memset(&t->matches[i], 0, sizeof(TMatch));
        t->matches[i].valid = 1;
        t->matches[i].next_match_idx = -1;
    }

    // Le tableau des matchs est arrangé en arbre parfait :
    // Les p/2 premiers matchs (indices 0 à p/2 - 1) sont le premier tour.
    // Leurs enfants vont dans (p/2) + i/2.
    int first_round_matches = p / 2;
    int current_offset = 0;
    int level_matches = first_round_matches;

    while (level_matches > 1) {
        for (int i = 0; i < level_matches; i++) {
            t->matches[current_offset + i].next_match_idx = current_offset + level_matches + (i / 2);
        }
        current_offset += level_matches;
        level_matches /= 2;
    }

    // Peupler le premier tour
    int player_idx = 0;
    for (int i = 0; i < first_round_matches; i++) {
        if (player_idx < t->player_count) {
            t->matches[i].p1 = t->players[player_idx].session_id;
            strncpy(t->matches[i].p1_name, t->players[player_idx].username, MAX_USERNAME_LEN - 1);
            player_idx++;
        }
        if (byes > 0) {
            // Un bye = le p2 est vide, p1 gagne tout de suite
            byes--;
            t->matches[i].p2 = 0;
            advance_winner(t, i, t->matches[i].p1, t->matches[i].p1_name);
        } else {
            if (player_idx < t->player_count) {
                t->matches[i].p2 = t->players[player_idx].session_id;
                strncpy(t->matches[i].p2_name, t->players[player_idx].username, MAX_USERNAME_LEN - 1);
                player_idx++;
            }
        }
    }

    // Lancer tous les matchs qui ont p1 et p2
    for (int i = 0; i < first_round_matches; i++) {
        if (t->matches[i].p1 != 0 && t->matches[i].p2 != 0 && t->matches[i].winner == 0) {
            send_start_room(t, i);
        }
    }
}

static void handle_create(PacketHeader *header, TournamentCreateReq *req) {
    for (int i = 0; i < MAX_TOURNAMENTS; i++) {
        if (tournaments[i].active == 0) {
            tournaments[i].active = 1;
            tournaments[i].id = next_tourney_id++;
            tournaments[i].max_players = req->max_players > MAX_T_PLAYERS ? MAX_T_PLAYERS : req->max_players;
            if (tournaments[i].max_players < 2) tournaments[i].max_players = 2;
            tournaments[i].player_count = 0;
            
            TournamentCreateResp resp;
            resp.tournament_id = tournaments[i].id;
            send_to_gateway(header->session_id, PACKET_TOURNAMENT_CREATE_RESP, &resp, sizeof(resp));
            printf("[Tournament] Created %d (max %d)\n", tournaments[i].id, tournaments[i].max_players);
            return;
        }
    }
}

static void handle_join(PacketHeader *header, TournamentJoinReq *req, const char* username) {
    TournamentJoinResp resp;
    memset(&resp, 0, sizeof(resp));

    for (int i = 0; i < MAX_TOURNAMENTS; i++) {
        if (tournaments[i].id == req->tournament_id && tournaments[i].active == 1) {
            Tournament *t = &tournaments[i];
            
            // Verifier duplicat
            for (int j = 0; j < t->player_count; j++) {
                if (t->players[j].session_id == header->session_id) {
                    resp.status = 1;
                    send_to_gateway(header->session_id, PACKET_TOURNAMENT_JOIN_RESP, &resp, sizeof(resp));
                    return;
                }
            }

            if (t->player_count >= t->max_players) {
                resp.status = 0;
                strcpy(resp.message, "Tournoi plein.");
                send_to_gateway(header->session_id, PACKET_TOURNAMENT_JOIN_RESP, &resp, sizeof(resp));
                return;
            }

            t->players[t->player_count].session_id = header->session_id;
            strncpy(t->players[t->player_count].username, username, MAX_USERNAME_LEN - 1);
            t->player_count++;

            resp.status = 1;
            send_to_gateway(header->session_id, PACKET_TOURNAMENT_JOIN_RESP, &resp, sizeof(resp));
            printf("[Tournament] Player %s joined %d (%d/%d)\n", username, t->id, t->player_count, t->max_players);

            if (t->player_count == t->max_players) {
                t->active = 2; // running
                build_bracket(t);
            }
            return;
        }
    }

    resp.status = 0;
    strcpy(resp.message, "Tournoi introuvable ou ferme.");
    send_to_gateway(header->session_id, PACKET_TOURNAMENT_JOIN_RESP, &resp, sizeof(resp));
}

static void handle_game_finished(GameFinished *fin) {
    for (int i = 0; i < MAX_TOURNAMENTS; i++) {
        if (tournaments[i].id == fin->tournament_id && tournaments[i].active == 2) {
            Tournament *t = &tournaments[i];
            // Trouver le match
            for (int m = 0; m < t->num_matches; m++) {
                TMatch *match = &t->matches[m];
                if (match->started && match->winner == 0) {
                    if (match->p1 == fin->winner_session_id || match->p2 == fin->winner_session_id) {
                        const char* winner_name = (match->p1 == fin->winner_session_id) ? match->p1_name : match->p2_name;
                        advance_winner(t, m, fin->winner_session_id, winner_name);
                        return;
                    }
                }
            }
        }
    }
}

int main(void) {
    printf("[Tournament Worker] Demarrage...\n");

    global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
    if (global_mq == -1) {
        perror("ipc_msg_get failed");
        exit(1);
    }

    memset(tournaments, 0, sizeof(tournaments));

    char msg_buffer[MAX_MSG_SIZE];
    while (1) {
        int nbytes = ipc_msg_receive(global_mq, msg_buffer, sizeof(msg_buffer), MSG_TYPE_TOURNAMENT);
        if (nbytes <= 0) continue;

        PacketHeader *header = (PacketHeader *)msg_buffer;
        void *payload = msg_buffer + sizeof(PacketHeader);

        switch (header->type) {
            case PACKET_TOURNAMENT_CREATE_REQ:
                handle_create(header, (TournamentCreateReq *)payload);
                break;
            case PACKET_TOURNAMENT_JOIN_REQ:
                // Pour récupérer le nom on doit gruger ou obliger le client à l'envoyer.
                // Dans matchmaker on avait le username dans le paquet req.
                // Ici on suppose le username = "PlayerX"
                {
                    char defaultName[32];
                    snprintf(defaultName, sizeof(defaultName), "Player_%u", header->session_id);
                    handle_join(header, (TournamentJoinReq *)payload, defaultName);
                }
                break;
            case PACKET_GAME_FINISHED:
                handle_game_finished((GameFinished *)payload);
                break;
            default:
                break;
        }
    }

    return 0;
}
