#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../common/ipc_utils/ipc_keys.h"
#include "../../common/ipc_utils/ipc_utils.h"
#include "../../common/network_models/packet_types.h"

#define MAX_TOURNAMENTS 10        // Nombre maximum de tournois
#define MAX_T_PLAYERS 32          // Joueurs max par tournoi
#define MAX_MATCHES (MAX_T_PLAYERS * 2) // Taille du tableau pour l'arbre binaire des matchs


typedef struct {
    uint32_t session_id;               // Identifiant
    char username[MAX_USERNAME_LEN];   // Nom d'utilisateur 
} TPlayer;


typedef struct {
    int valid;                         // Indique si le match fait partie du bracket actuel
    uint32_t p1;                       // Session ID du joueur 1 (Blanc)
    uint32_t p2;                       // Session ID du joueur 2 (Noir)
    char p1_name[MAX_USERNAME_LEN];
    char p2_name[MAX_USERNAME_LEN];
    uint32_t winner;                   // Session ID du vainqueur
    int next_match_idx;                
    int started;                       // Flag pour éviter de relancer un match déjà en cours
} TMatch;


typedef struct {
    uint32_t id;                       
    int active;                        // État : 0=Inactif, 1=Attente joueurs, 2=En cours
    int max_players;                   // Limite de participants
    TPlayer players[MAX_T_PLAYERS];    // Liste inscrits
    int player_count;                  // Nombre d'inscrits
    TMatch matches[MAX_MATCHES];       // arbre des rencontres
    int num_matches;                   // Nombre total de matchs calculés pour ce bracket
} Tournament;

static Tournament tournaments[MAX_TOURNAMENTS];
static uint32_t next_tourney_id = 1;
static uint32_t next_room_id = 10000;  // Gros nombre sinon collisions
static int global_mq = -1;             // File de Messages IPC


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
    // Envoi via IPC avec le type MSG_TYPE_GATEWAY
    ipc_msg_send(global_mq, out_buf, header->length, MSG_TYPE_GATEWAY);
}


static void broadcast_tournament_state(Tournament *t) {
    TournamentStatePacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.tournament_id = t->id;
    pkt.joined = (uint8_t)t->player_count;
    pkt.max = (uint8_t)t->max_players;
    pkt.status = (uint8_t)(t->active == 2 ? 1 : 0); // 0=Attente, 1=En cours
    pkt.winner_name[0] = '\0';

    for (int i = 0; i < t->player_count; i++) {
        send_to_gateway(t->players[i].session_id, PACKET_TOURNAMENT_STATE, &pkt, sizeof(pkt));
    }
}


static void send_start_room(Tournament *t, int match_index) {
    TMatch *m = &t->matches[match_index];
    // Sécurité : ne pas lancer si le match est déjà en cours ou incomplet
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

    // Préparation pour le Joueur 1 (Blanc)
    out_header->session_id = m->p1;
    started->opponent_session_id = m->p2;
    started->your_color = 0;
    strncpy(started->opponent_username, m->p2_name, sizeof(started->opponent_username) - 1);
    ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GATEWAY);

    // Préparation pour le Joueur 2 (Noir)
    out_header->session_id = m->p2;
    started->opponent_session_id = m->p1;
    started->your_color = 1;
    strncpy(started->opponent_username, m->p1_name, sizeof(started->opponent_username) - 1);
    ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GATEWAY);

    out_header->session_id = m->p1;
    ipc_msg_send(global_mq, out_buf, out_header->length, MSG_TYPE_GAMEWORKER);
    
    printf("[Tournament] Match lance dans la salle %d (Tournoi %d)\n", room_id, t->id);
}


static void advance_winner(Tournament *t, int match_index, uint32_t winner_id, const char* winner_name) {
    TMatch *m = &t->matches[match_index];
    m->winner = winner_id;
    
    // Cas de la Finale terminée
    if (m->next_match_idx == -1) {
        printf("[Tournament] Tournoi %d termine ! Vainqueur : %s\n", t->id, winner_name);
        
        // Notification générale de fin de tournoi
        TournamentStatePacket pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.tournament_id = t->id;
        pkt.joined = (uint8_t)t->player_count;
        pkt.max = (uint8_t)t->max_players;
        pkt.status = 2; // Finished
        strncpy(pkt.winner_name, winner_name, MAX_USERNAME_LEN - 1);

        for (int i = 0; i < t->player_count; i++) {
            send_to_gateway(t->players[i].session_id, PACKET_TOURNAMENT_STATE, &pkt, sizeof(pkt));
        }

        // Libération de l'emplacement du tournoi
        memset(t, 0, sizeof(Tournament));
        return;
    }

    // Progression vers le match suivant dans le bracket
    TMatch *next_m = &t->matches[m->next_match_idx];
    if (next_m->p1 == 0) {
        next_m->p1 = winner_id;
        strncpy(next_m->p1_name, winner_name, MAX_USERNAME_LEN - 1);
    } else {
        next_m->p2 = winner_id;
        strncpy(next_m->p2_name, winner_name, MAX_USERNAME_LEN - 1);
    }

    // Si les deux adversaires du match suivant sont connus, on le lance
    if (next_m->p1 != 0 && next_m->p2 != 0) {
        send_start_room(t, m->next_match_idx);
    }
}


static void build_bracket(Tournament *t) {
    // Calcul de la taille de l'arbre
    int p = next_power_of_2(t->player_count);
    int byes = p - t->player_count;
    int num_matches = p - 1;
    t->num_matches = num_matches;

    // Initialisation des matchs
    for (int i = 0; i < num_matches; i++) {
        memset(&t->matches[i], 0, sizeof(TMatch));
        t->matches[i].valid = 1;
        t->matches[i].next_match_idx = -1;
    }

    
    int first_round_matches = p / 2;
    int current_offset = 0;
    int level_matches = first_round_matches;

    // Liaison des  enfants vers leurs parents
    while (level_matches > 1) {
        for (int i = 0; i < level_matches; i++) {
            t->matches[current_offset + i].next_match_idx = current_offset + level_matches + (i / 2);
        }
        current_offset += level_matches;
        level_matches /= 2;
    }

    // Remplissage du premier tour avec les joueurs inscrits
    int player_idx = 0;
    for (int i = 0; i < first_round_matches; i++) {
        // Joueur 1 du match i
        if (player_idx < t->player_count) {
            t->matches[i].p1 = t->players[player_idx].session_id;
            strncpy(t->matches[i].p1_name, t->players[player_idx].username, MAX_USERNAME_LEN - 1);
            player_idx++;
        }
        
        if (byes > 0) {
            byes--;
            t->matches[i].p2 = 0; // Pas d'adversaire
            // Qualification automatique pour le tour suivant
            advance_winner(t, i, t->matches[i].p1, t->matches[i].p1_name);
        } else {
            if (player_idx < t->player_count) {
                t->matches[i].p2 = t->players[player_idx].session_id;
                strncpy(t->matches[i].p2_name, t->players[player_idx].username, MAX_USERNAME_LEN - 1);
                player_idx++;
            }
        }
    }

    // Lancement des matchs du 1er tour qui ont deux adversaires
    for (int i = 0; i < first_round_matches; i++) {
        if (t->matches[i].p1 != 0 && t->matches[i].p2 != 0 && t->matches[i].winner == 0) {
            send_start_room(t, i);
        }
    }
}


static void handle_create(PacketHeader *header, TournamentCreateReq *req) {
    for (int i = 0; i < MAX_TOURNAMENTS; i++) {
        if (tournaments[i].active == 0) { // On cherche un emplacement libre
            tournaments[i].active = 1;
            tournaments[i].id = next_tourney_id++;
            // Bornage du nombre de joueurs
            tournaments[i].max_players = req->max_players > MAX_T_PLAYERS ? MAX_T_PLAYERS : req->max_players;
            if (tournaments[i].max_players < 2) tournaments[i].max_players = 2;
            tournaments[i].player_count = 0;
            
            TournamentCreateResp resp;
            resp.tournament_id = tournaments[i].id;
            send_to_gateway(header->session_id, PACKET_TOURNAMENT_CREATE_RESP, &resp, sizeof(resp));
            printf("[Tournament] Tournoi cree : ID=%d (Max=%d)\n", tournaments[i].id, tournaments[i].max_players);
            return;
        }
    }
}


static void handle_join(PacketHeader *header, TournamentJoinReq *req) {
    TournamentJoinResp resp;
    memset(&resp, 0, sizeof(resp));

    for (int i = 0; i < MAX_TOURNAMENTS; i++) {
        if (tournaments[i].id == req->tournament_id) {
            Tournament *t = &tournaments[i];

            // On ne peut pas rejoindre un tournoi qui a déjà débuté
            if (t->active == 2) {
                resp.status = 0;
                strcpy(resp.message, "Le tournoi a deja commence.");
                send_to_gateway(header->session_id, PACKET_TOURNAMENT_JOIN_RESP, &resp, sizeof(resp));
                return;
            }

            if (t->active != 1) continue;
            
            // Vérification des doublons
            for (int j = 0; j < t->player_count; j++) {
                if (t->players[j].session_id == header->session_id) {
                    resp.status = 1; // Déjà inscrit
                    send_to_gateway(header->session_id, PACKET_TOURNAMENT_JOIN_RESP, &resp, sizeof(resp));
                    return;
                }
            }

            // Vérification de la capacité
            if (t->player_count >= t->max_players) {
                resp.status = 0;
                strcpy(resp.message, "Tournoi plein.");
                send_to_gateway(header->session_id, PACKET_TOURNAMENT_JOIN_RESP, &resp, sizeof(resp));
                return;
            }

            // Ajout du joueur
            t->players[t->player_count].session_id = header->session_id;
            strncpy(t->players[t->player_count].username, req->username, MAX_USERNAME_LEN - 1);
            t->player_count++;

            resp.status = 1;
            send_to_gateway(header->session_id, PACKET_TOURNAMENT_JOIN_RESP, &resp, sizeof(resp));
            printf("[Tournament] Joueur %s a rejoint %d (%d/%d)\n", req->username, t->id, t->player_count, t->max_players);

            broadcast_tournament_state(t);

            // Si le tournoi est plein, on lance le bracket
            if (t->player_count == t->max_players) {
                t->active = 2;
                build_bracket(t);
            }
            return;
        }
    }

    resp.status = 0;
    strcpy(resp.message, "Tournoi introuvable.");
    send_to_gateway(header->session_id, PACKET_TOURNAMENT_JOIN_RESP, &resp, sizeof(resp));
}


static void handle_list(PacketHeader *header) {
    TournamentListResp resp;
    memset(&resp, 0, sizeof(resp));
    
    for (int i = 0; i < MAX_TOURNAMENTS; i++) {
        // Seuls les tournois en attente sont affichés
        if (tournaments[i].active == 1 && resp.tournament_count < 10) {
            resp.tournaments[resp.tournament_count].tournament_id = tournaments[i].id;
            resp.tournaments[resp.tournament_count].player_count = (uint8_t)tournaments[i].player_count;
            resp.tournaments[resp.tournament_count].max_players = (uint8_t)tournaments[i].max_players;
            resp.tournament_count++;
        }
    }
    
    send_to_gateway(header->session_id, PACKET_TOURNAMENT_LIST_RESP, &resp, sizeof(resp));
}


static void handle_game_finished(GameFinished *fin) {
    for (int i = 0; i < MAX_TOURNAMENTS; i++) {
        // On ne regarde que les tournois en cours de compétition
        if (tournaments[i].id == fin->tournament_id && tournaments[i].active == 2) {
            Tournament *t = &tournaments[i];
            
            // Recherche du match spécifique dans le bracket
            for (int m = 0; m < t->num_matches; m++) {
                TMatch *match = &t->matches[m];
                if (match->started && match->winner == 0) {
                    // Si l'un des joueurs du match correspond au gagnant signalé
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
    printf("[Tournament Worker] Demarrage de la boucle d'evenements...\n");

    // Connexion à la file de messages globale
    global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
    if (global_mq == -1) {
        perror("ipc_msg_get fail");
        exit(1);
    }

    // Initialisation de la mémoire des tournois
    memset(tournaments, 0, sizeof(tournaments));

    char msg_buffer[MAX_MSG_SIZE];
    while (1) {
        // Lecture bloquante des messages de type MSG_TYPE_TOURNAMENT
        int nbytes = ipc_msg_receive(global_mq, msg_buffer, sizeof(msg_buffer), MSG_TYPE_TOURNAMENT);
        if (nbytes <= 0) continue;

        PacketHeader *header = (PacketHeader *)msg_buffer;
        void *payload = msg_buffer + sizeof(PacketHeader);

        switch (header->type) {
            case PACKET_TOURNAMENT_CREATE_REQ:
                handle_create(header, (TournamentCreateReq *)payload);
                break;
            case PACKET_TOURNAMENT_JOIN_REQ:
                handle_join(header, (TournamentJoinReq *)payload);
                break;
            case PACKET_TOURNAMENT_LIST_REQ:
                handle_list(header);
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
