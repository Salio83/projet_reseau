#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "../../common/ipc_utils/ipc_utils.h"
#include "../../common/ipc_utils/ipc_keys.h"
#include "../../common/network_models/packet_types.h"

/**
 * @file matchmaker_main.c
 * @brief Service de mise en relation des joueurs (Matchmaking).
 * 
 * Ce service gère une file d'attente de joueurs cherchant une partie.
 * Dès que deux joueurs sont disponibles, il crée une nouvelle partie et
 * en informe le GameWorker ainsi que les deux clients concernés.
 */

#define MAX_WAITING 100

int main() {
    printf("[Matchmaker] Démarrage (version sécurisée)...\n");

    // Récupération de l'identifiant de la file de messages globale
    int global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
    if (global_mq == -1) {
        perror("ipc_msg_get failed");
        exit(1);
    }

    // Tableau stockant les IDs (fd) des joueurs en attente
    uint32_t waiting_players[MAX_WAITING];
    int waiting_count = 0;
    uint32_t next_game_id = 1; // Compteur pour générer des IDs de partie uniques

    char msg_buffer[MAX_MSG_SIZE];

    while (1) {
        // Lecture bloquante des demandes de matchmaking destinées à ce service (MSG_TYPE_MATCHMAKING)
        int nbytes = ipc_msg_receive(global_mq, msg_buffer, MAX_MSG_SIZE, MSG_TYPE_MATCHMAKING);
        if (nbytes > 0) {
            PacketHeader* header = (PacketHeader*)msg_buffer;
            
            // 1. Sécurité : Vérifier si le joueur est déjà dans la file d'attente
            int already_waiting = -1;
            for(int i=0; i < waiting_count; i++) {
                if(waiting_players[i] == header->client_id) {
                    already_waiting = i;
                    break;
                }
            }

            if (already_waiting != -1) {
                // Le joueur tente de rejoindre deux fois, on ignore
                continue;
            }

            printf("[Matchmaker] Joueur %d rejoint la file (Attente: %d)\n", header->client_id, waiting_count + 1);
            waiting_players[waiting_count++] = header->client_id;

            // 2. Logique de Matchmaking : Si on a au moins 2 joueurs, on crée un match
            if (waiting_count >= 2) {
                uint32_t p1 = waiting_players[0]; // Joueur Blanc (le premier arrivé)
                uint32_t p2 = waiting_players[1]; // Joueur Noir

                if (p1 == p2) {
                    // Sécurité redondante
                    waiting_count = 1;
                    continue;
                }

                uint32_t gid = next_game_id++;
                printf("[Matchmaker] MATCH CRÉÉ ! Game %d: %d vs %d\n", gid, p1, p2);

                char out_buf[MAX_MSG_SIZE];
                PacketHeader* out_h = (PacketHeader*)out_buf;
                GameStarted* gs = (GameStarted*)(out_buf + sizeof(PacketHeader));

                out_h->type = PACKET_GAME_STARTED;
                out_h->length = sizeof(PacketHeader) + sizeof(GameStarted);

                // --- Notification Joueur 1 (Blanc) via Gateway ---
                out_h->client_id = p1;
                gs->game_id = gid; gs->opponent_id = p2; gs->your_color = 0; 
                ipc_msg_send(global_mq, out_buf, out_h->length, MSG_TYPE_GATEWAY);

                // --- Notification Joueur 2 (Noir) via Gateway ---
                out_h->client_id = p2;
                gs->game_id = gid; gs->opponent_id = p1; gs->your_color = 1;
                ipc_msg_send(global_mq, out_buf, out_h->length, MSG_TYPE_GATEWAY);

                // --- Notification du GameWorker via la file globale ---
                out_h->client_id = p1; // Par convention, on utilise l'ID du blanc ici
                gs->game_id = gid; gs->opponent_id = p2; gs->your_color = 0;
                ipc_msg_send(global_mq, out_buf, out_h->length, MSG_TYPE_GAMEWORKER);

                // --- Nettoyage de la file d'attente ---
                waiting_count -= 2;
                for (int i = 0; i < waiting_count; i++) {
                    waiting_players[i] = waiting_players[i + 2];
                }
            }
        }
    }
    return 0;
}
