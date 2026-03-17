#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <poll.h>

#include "packet_types.h"
#include "ipc_utils.h"
#include "ipc_keys.h"

#define PORT 8080
#define MAX_CLIENTS 100

typedef struct {
    int socket;
    uint32_t client_id;
} ClientContext;

ClientContext clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int auth_msqid, matchmaking_msqid, game_msqid, chat_msqid, gateway_msqid;

void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    PacketHeader header;
    while (read(client_socket, &header, sizeof(PacketHeader)) > 0) {
        printf("Reçu paquet type %d de client %d\n", header.type, header.client_id);
        
        char buffer[MAX_MSG_SIZE];
        memcpy(buffer, &header, sizeof(PacketHeader));
        
        if (header.length > sizeof(PacketHeader)) {
            size_t payload_size = header.length - sizeof(PacketHeader);
            if (payload_size > MAX_MSG_SIZE - sizeof(PacketHeader)) {
                fprintf(stderr, "Payload too large\n");
                break;
            }
            read(client_socket, buffer + sizeof(PacketHeader), payload_size);
        }

        int target_msqid = -1;
        switch (header.type) {
            case PACKET_AUTH_REQ:
                target_msqid = auth_msqid;
                break;
            case PACKET_MATCHMAKING_REQ:
                target_msqid = matchmaking_msqid;
                break;
            case PACKET_PLAYER_MOVE:
                target_msqid = game_msqid;
                break;
            case PACKET_CHAT_MSG:
                target_msqid = chat_msqid;
                break;
            default:
                printf("Type de paquet inconnu : %d\n", header.type);
                break;
        }

        if (target_msqid != -1) {
            ipc_msg_send(target_msqid, buffer, header.length, 1);
        }
    }

    printf("Client déconnecté\n");
    close(client_socket);
    // TODO: retirer de la liste des clients
    return NULL;
}

// Thread qui écoute les réponses des services
void *response_handler(void *arg) {
    (void)arg;
    char buffer[MAX_MSG_SIZE];
    while (1) {
        int nbytes = ipc_msg_receive(gateway_msqid, buffer, MAX_MSG_SIZE, 0);
        if (nbytes > 0) {
            PacketHeader *header = (PacketHeader *)buffer;
            printf("Réponse de service reçue pour client %d (type %d)\n", header->client_id, header->type);
            
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].client_id == header->client_id && clients[i].socket != 0) {
                    send(clients[i].socket, buffer, header->length, 0);
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
        }
    }
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Initialisation IPC
    auth_msqid = ipc_msg_get(GET_AUTH_KEY());
    matchmaking_msqid = ipc_msg_get(GET_MATCHMAKING_KEY());
    game_msqid = ipc_msg_get(GET_GAME_WORKER_KEY());
    chat_msqid = ipc_msg_get(GET_CHAT_KEY());
    gateway_msqid = ipc_msg_get(GET_GATEWAY_KEY());

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
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

    printf("Gateway World Polytech Chess en ligne sur le port %d\n", PORT);

    pthread_t resp_tid;
    pthread_create(&resp_tid, NULL, response_handler, NULL);

    uint32_t next_client_id = 1;

    while(1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        printf("Nouveau client connecté ! (IP: %s)\n", inet_ntoa(address.sin_addr));
        
        pthread_mutex_lock(&clients_mutex);
        int added = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == 0) {
                clients[i].socket = new_socket;
                clients[i].client_id = next_client_id++;
                
                pthread_t tid;
                int *p_sock = malloc(sizeof(int));
                *p_sock = new_socket;
                pthread_create(&tid, NULL, client_handler, p_sock);
                added = 1;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (!added) {
            printf("Serveur plein, rejet d'un client.\n");
            close(new_socket);
        }
    }

    return 0;
}
