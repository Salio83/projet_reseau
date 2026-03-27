#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common/network_models/packet_types.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6767

int main() {
  int sock = 0;
  struct sockaddr_in serv_addr;

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("\n Erreur de création du socket \n");
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);

  if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
    printf("\n Adresse invalide ou non supportée \n");
    return -1;
  }

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("\n Connexion échouée \n");
    return -1;
  }

  printf("[Client] Connecté au Gateway sur le port %d\n", SERVER_PORT);

  // Préparation d'une requête d'authentification
  PacketHeader header;
  header.type = PACKET_AUTH_REQ;
  header.length = sizeof(PacketHeader) + sizeof(AuthRequest);
  header.client_id = 0; // Sera rempli par le Gateway

  AuthRequest auth;
  strncpy(auth.username, "Joueur_Test", 32);
  strncpy(auth.password_hash, "hash_secret_123", 64);

  // Envoi du Header
  send(sock, &header, sizeof(PacketHeader), 0);
  // Envoi du Payload
  send(sock, &auth, sizeof(AuthRequest), 0);

  printf("[Client] Paquet PACKET_AUTH_REQ envoyé pour l'utilisateur: %s\n",
         auth.username);

  // Attente de la réponse du Gateway (redirigée depuis l'Auth Service)
  char buffer[2048];
  int valread = read(sock, buffer, 2048);
  if (valread > 0) {
    PacketHeader *res_header = (PacketHeader *)buffer;
    printf("[Client] Réponse reçue du Gateway ! Type: %d, Taille: %d\n",
           res_header->type, res_header->length);

    if (res_header->type == PACKET_AUTH_REQ) {
      AuthRequest *res_auth = (AuthRequest *)(buffer + sizeof(PacketHeader));
      printf("[Client] Confirmation pour l'utilisateur: %s\n",
             res_auth->username);
    }
  }

  close(sock);
  return 0;
}
