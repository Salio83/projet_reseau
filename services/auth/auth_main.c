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

    // Création préventive du fichier pour ftok (si pas déjà créé par le Gateway)
    int fd = open(AUTH_MSG_QUEUE_PATH, O_CREAT | O_RDWR, 0666);
    if (fd != -1) close(fd);

    // Récupération de l'identifiant de la file de messages du service Auth
    int auth_mq = ipc_msg_get(ipc_get_key(AUTH_MSG_QUEUE_PATH, AUTH_MSG_QUEUE_ID));
    if (auth_mq == -1) {
        perror("ipc_msg_get failed");
        exit(1);
    }

    printf("[Auth Service] En attente de messages sur la file %d...\n", auth_mq);

    char msg_buffer[MAX_MSG_SIZE];
    while (1) {
        // Lecture bloquante des requêtes d'authentification envoyées par le Gateway
        int nbytes = ipc_msg_receive(auth_mq, msg_buffer, MAX_MSG_SIZE, 1);
        if (nbytes > 0) {
            PacketHeader* header = (PacketHeader*)msg_buffer;
            AuthRequest* req = (AuthRequest*)(msg_buffer + sizeof(PacketHeader));

            printf("[Auth Service] Requête reçue de client_id %d\n", header->client_id);
            printf("[Auth Service] Username: %s\n", req->username);
            
            // --- Simulation de validation ---
            // Dans une version finale, on vérifierait ici le mot de passe dans une base de données.
            printf("[Auth Service] Authentification réussie pour %s\n", req->username);
            
            // --- Envoi d'une réponse au Gateway ---
            // On récupère la file de retour (gateway_mq)
            int gateway_mq = ipc_msg_get(ipc_get_key(GATEWAY_MSG_QUEUE_PATH, GATEWAY_MSG_QUEUE_ID));
            
            // On renvoie le paquet au Gateway. Le Gateway saura à quel client répondre
            // grâce au 'client_id' (fd) que nous avons conservé dans le header.
            ipc_msg_send(gateway_mq, msg_buffer, header->length, 1);
            printf("[Auth Service] Réponse envoyée au Gateway pour client_id %d\n", header->client_id);
        }
    }

    return 0;
}
