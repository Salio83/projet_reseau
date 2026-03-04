#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define TCP_PORT 8080
#define UDP_PORT 8081
#define MAX_CLIENTS 1000

/* --- Fonctions d'initialisation réseau --- */

int init_tcp_server() {
    // Création et configuration d'un socket TCP standard
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    // Le TCP sera utilisé pour les coups d'échecs afin de garantir que 
    // la partie ne soit pas désynchronisée
    return server_fd;
}

int init_udp_server() {
    // Création et configuration d'un socket UDP standard
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);

    return server_fd;
}

/* --- Routage des paquets --- */

void route_tcp_packet(char* buffer, int client_socket) {
    // Logique de lecture du paquet (ex: struct PlayerMove) [cite: 63]
    
    // Si c'est une demande de matchmaking :
    // On dépose le message dans la file (Message Queue) destinée au processus Matchmaker[cite: 35, 36, 37].
    // send_to_matchmaker_queue(buffer);

    // Si c'est un coup d'échecs ou du chat :
    // On route vers le Game Worker ou le module Chat via IPC[cite: 13, 20].
    
    printf("Paquet TCP reçu et routé de manière fiable.\n");
}

void broadcast_to_spectators_udp(int udp_socket, char* game_state) {
    // Envoi de l'état de l'échiquier et du chronomètre en UDP[cite: 53].
    // L'envoi se fait sans vérifier si les données sont bien arrivées[cite: 51].
    // Si un paquet est perdu, le suivant arrivera peu après[cite: 54].
    
    // (Boucle fictive sur la liste des spectateurs)
    // for (int i = 0; i < nb_spectateurs; i++) {
    //     sendto(udp_socket, game_state, ...);
    // }
    printf("Mise à jour envoyée aux spectateurs via UDP.\n");
}

/* --- Boucle principale --- */

void handle_connections(int tcp_socket, int udp_socket) {
    // Utilisation typique de select() ou epoll() pour écouter plusieurs 
    // sockets en même temps sans bloquer le thread.
    
    while(1) {
        // 1. Vérifier si un nouveau client TCP se connecte (Joueur)
        // accept(...)
        
        // 2. Vérifier si des données TCP arrivent (Coups, Auth, Chat) [cite: 47]
        // read(...) -> route_tcp_packet(...)
        
        // 3. Diffuser régulièrement l'état aux spectateurs en UDP (ex: 10 fois par seconde) [cite: 53]
        // Si l'orchestrateur met à jour la mémoire partagée, le gateway peut
        // la lire instantanément et l'envoyer[cite: 41, 42].
        // char* current_state = read_shared_memory_state();
        // broadcast_to_spectators_udp(udp_socket, current_state);
        
        break; // Break temporaire pour l'exemple
    }
}