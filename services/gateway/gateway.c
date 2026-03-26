#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "gateway.h"
#include "../../common/network_models/packet_types.h"
#include "../../common/ipc_utils/ipc_utils.h"
#include "../../common/ipc_utils/ipc_keys.h"

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 2048

// Identifiant unique pour la file de messages globale
int global_mq;

/**
 * @brief Prépare l'environnement IPC en créant le fichier nécessaire et en récupérant la file.
 */
void setup_ipc() {
    // Création préventive du fichier pour ftok s'il n'existe pas sur le disque
    int fd = open(GLOBAL_MSG_QUEUE_PATH, O_CREAT | O_RDWR, 0666);
    if (fd != -1) close(fd);

    // Récupération de l'ID de la file globale
    global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
}

/**
 * @brief Analyse un paquet réseau et l'envoie vers le service IPC approprié via la file unique.
 */
void route_packet(PacketHeader* header, char* payload, int client_fd) {
    long target_type = -1;
    
    // Routage basé sur le type de paquet
    switch (header->type) {
        case PACKET_AUTH_REQ:
            target_type = MSG_TYPE_AUTH;
            break;
        case PACKET_MATCHMAKING_REQ:
            target_type = MSG_TYPE_MATCHMAKING;
            break;
        case PACKET_PLAYER_MOVE:
            target_type = MSG_TYPE_GAMEWORKER;
            break;
        case PACKET_CHAT_MSG:
            target_type = MSG_TYPE_CHAT;
            break;
        default:
            printf("[Gateway] Type de paquet inconnu: %d\n", header->type);
            return;
    }

    if (target_type != -1) {
        header->client_id = client_fd; 
        
        size_t payload_len = header->length - sizeof(PacketHeader);
        char msg_buffer[MAX_MSG_SIZE];
        memcpy(msg_buffer, header, sizeof(PacketHeader));
        if (payload_len > 0) {
            memcpy(msg_buffer + sizeof(PacketHeader), payload, payload_len);
        }
        
        // Envoi vers le service spécifié par son mtype dans la file globale
        ipc_msg_send(global_mq, msg_buffer, header->length, target_type);
    }
}

/**
 * @brief Vérifie s'il y a des messages pour la Gateway dans la file unique.
 */
void handle_service_responses() {
    char msg_buffer[MAX_MSG_SIZE];
    
    // Lecture non-bloquante des messages destinés à la Gateway (MSG_TYPE_GATEWAY)
    while (ipc_msg_receive_nowait(global_mq, msg_buffer, MAX_MSG_SIZE, MSG_TYPE_GATEWAY) != -1) {
        PacketHeader* header = (PacketHeader*)msg_buffer;
        int client_fd = header->client_id;
        
        printf("[Gateway] Envoi réponse au client fd %d (Type: %d, Length: %d)\n", client_fd, header->type, header->length);
        send(client_fd, msg_buffer, header->length, 0);
    }
}

/**
 * @brief Boucle principale utilisant select() pour gérer multiplexage réseau et IPC.
 */
void start_gateway() {
    int server_fd, new_socket, client_socket[MAX_CLIENTS], max_sd, sd;
    struct sockaddr_in address;
    fd_set readfds;

    setup_ipc();

    for (int i = 0; i < MAX_CLIENTS; i++) client_socket[i] = 0;

    // Création du socket serveur TCP
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Gateway en ligne sur le port %d\n", PORT);

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // Ajout des sockets clients existants à la liste de surveillance
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i];
            if(sd > 0) FD_SET(sd, &readfds);
            if(sd > max_sd) max_sd = sd;
        }

        // Timeout court pour alterner entre réseau et lecture des Message Queues
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms

        int activity = select(max_sd + 1, &readfds, NULL, NULL, &tv);

        if ((activity < 0) && (errno != EINTR)) {
            printf("select error");
        }

        // --- 1. Gérer les réponses provenant des services internes ---
        handle_service_responses();

        // --- 2. Gérer les nouvelles demandes de connexion ---
        if (FD_ISSET(server_fd, &readfds)) {
            int addrlen = sizeof(address);
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }
            printf("Nouveau client : fd %d\n", new_socket);
            
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if( client_socket[i] == 0 ) {
                    client_socket[i] = new_socket;
                    break;
                }
            }
        }

        // --- 3. Gérer les paquets reçus des clients connectés ---
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i];

            if (FD_ISSET(sd, &readfds)) {
                PacketHeader header;
                // Lecture de l'entête pour connaître la taille totale du message
                int valread = read(sd, &header, sizeof(PacketHeader));
                if (valread <= 0) {
                    // Déconnexion détectée
                    printf("Client déconnecté : fd %d\n", sd);
                    close(sd);
                    client_socket[i] = 0;
                } else {
                    // Lecture du corps (payload) si nécessaire
                    char payload[BUFFER_SIZE];
                    int payload_len = header.length - sizeof(PacketHeader);
                    if (payload_len > 0 && payload_len < BUFFER_SIZE) {
                        read(sd, payload, payload_len);
                    }
                    // Envoi vers les services via IPC
                    route_packet(&header, payload, sd);
                }
            }
        }
    }
}
