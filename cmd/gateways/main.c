#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
/* Inclusions fictives basées sur la structure du projet */
// #include "../network_models/packet_types.h" 
// #include "../ipc_utils/message_queue.h"

/* Fonctions définies dans connection.c */
extern int init_tcp_server();
extern int init_udp_server();
extern void handle_connections(int tcp_socket, int udp_socket);

int main() {
    printf("Démarrage du Gateway (Routeur TCP/UDP) - Serveur World Polytech Chess...\n");

    // 1. Initialisation de l'IPC (Files de messages)
    // Utile pour envoyer les requêtes au Matchmaker de manière asynchrone
    // afin de ne pas bloquer le serveur si 1000 joueurs se connectent[cite: 37, 38].
    // init_ipc_queues(); 

    // 2. Initialisation des sockets réseaux
    int tcp_socket = init_tcp_server();
    int udp_socket = init_udp_server();

    if (tcp_socket < 0 || udp_socket < 0) {
        fprintf(stderr, "Erreur lors de l'initialisation des sockets.\n");
        return EXIT_FAILURE;
    }

    printf("Gateway en écoute sur les ports TCP et UDP...\n");

    // 3. Boucle principale de gestion des clients
    // Cette fonction va écouter les joueurs (TCP) et les spectateurs (UDP)[cite: 2, 3, 4].
    handle_connections(tcp_socket, udp_socket);

    // 4. Nettoyage
    close(tcp_socket);
    close(udp_socket);
    return EXIT_SUCCESS;
}