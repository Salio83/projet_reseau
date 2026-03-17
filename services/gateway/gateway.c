#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "gateway.h"

#define PORT 8080
#define MAX_CLIENTS 100

void start_gateway() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // 1. Création de la socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return;
    }

    // 2. Attachement au port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        return;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return;
    }

    // 3. Ecoute
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        return;
    }

    printf("Gateway en ligne sur le port %d (Mode Unifié)\n", PORT);
    printf("En attente de connexions...\n");

    while(1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        printf("Nouveau client connecté ! (IP: %s)\n", inet_ntoa(address.sin_addr));
        
        // Pour l'instant, on ferme juste la connexion après avoir dit bonjour
        char *hello = "Bienvenue sur World Polytech Chess Unified Server!\n";
        send(new_socket, hello, strlen(hello), 0);
        close(new_socket);
    }
}
