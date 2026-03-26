#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "../../common/ipc_utils/ipc_utils.h"
#include "../../common/ipc_utils/ipc_keys.h"
#include "../../common/network_models/packet_types.h"

/**
 * @file auth_main.c
 * @brief Service d'authentification des joueurs.
 * 
 * Ce service gère la validation des identifiants des joueurs lors de leur connexion.
 * Il communique avec le Gateway via des Message Queues IPC.
 */

int main() {
    printf("[Auth Service] Démarrage...\n");

    // Création préventive du fichier pour ftok
    int fd = open(GLOBAL_MSG_QUEUE_PATH, O_CREAT | O_RDWR, 0666);
    if (fd != -1) close(fd);

    // Récupération de l'identifiant de la file de messages globale
    int global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
    if (global_mq == -1) {
        perror("ipc_msg_get failed");
        exit(1);
    }

    printf("[Auth Service] En attente de messages (Type: %d) sur la file globale...\n", MSG_TYPE_AUTH);

    char msg_buffer[MAX_MSG_SIZE];
    while (1) {
        // Lecture bloquante des requêtes d'authentification destinées à ce service (MSG_TYPE_AUTH)
        int nbytes = ipc_msg_receive(global_mq, msg_buffer, MAX_MSG_SIZE, MSG_TYPE_AUTH);
        if (nbytes > 0) {
            PacketHeader* header = (PacketHeader*)msg_buffer;
            AuthRequest* req = (AuthRequest*)(msg_buffer + sizeof(PacketHeader));

            printf("[Auth Service] Requête reçue de client_id %d\n", header->client_id);
            printf("[Auth Service] Username: %s\n", req->username);
            
            // --- Simulation de validation ---
            printf("[Auth Service] Authentification réussie pour %s\n", req->username);
            
            // --- Envoi d'une réponse au Gateway ---
            // On renvoie le paquet au Gateway en utilisant MSG_TYPE_GATEWAY
            ipc_msg_send(global_mq, msg_buffer, header->length, MSG_TYPE_GATEWAY);
            printf("[Auth Service] Réponse envoyée au Gateway pour client_id %d\n", header->client_id);
        }
    }

    return 0;
}
