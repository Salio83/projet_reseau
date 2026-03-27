#include "../common/network_models/packet_types.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 6767

int create_client() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);
  connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  return sock;
}

int main() {
  printf("--- Test Flux de Jeu (Matchmaking + Moves) ---\n");

  int c1 = create_client();
  int c2 = create_client();

  printf("[Client 1] Envoi PACKET_MATCHMAKING_REQ\n");
  PacketHeader h1 = {PACKET_MATCHMAKING_REQ, sizeof(PacketHeader), 0};
  send(c1, &h1, sizeof(h1), 0);

  printf("[Client 2] Envoi PACKET_MATCHMAKING_REQ\n");
  PacketHeader h2 = {PACKET_MATCHMAKING_REQ, sizeof(PacketHeader), 0};
  send(c2, &h2, sizeof(h2), 0);

  // Attente GameStarted
  char buf[1024];
  read(c1, buf, 1024);
  PacketHeader *res_h = (PacketHeader *)buf;
  if (res_h->type == PACKET_GAME_STARTED) {
    GameStarted *gs = (GameStarted *)(buf + sizeof(PacketHeader));
    printf("[Client 1] Partie démarrée ! ID: %d, Couleur: %s\n", gs->game_id,
           gs->your_color == 0 ? "Blanc" : "Noir");

    uint32_t gid = gs->game_id;

    // Tenter un coup (e2 -> e4)
    printf("[Client 1] Envoi d'un coup e2 -> e4\n");
    PacketHeader move_h = {PACKET_PLAYER_MOVE,
                           sizeof(PacketHeader) + sizeof(PlayerMove), 0};
    PlayerMove move = {gid, "e2", "e4", '\0'};

    char move_pkt[sizeof(PacketHeader) + sizeof(PlayerMove)];
    memcpy(move_pkt, &move_h, sizeof(move_h));
    memcpy(move_pkt + sizeof(move_h), &move, sizeof(move));
    send(c1, move_pkt, sizeof(move_pkt), 0);

    // Attente GameState Update
    read(c1, buf, 1024);
    res_h = (PacketHeader *)buf;
    if (res_h->type == PACKET_GAME_STATE_UDP) {
      GameStateUDP *gsu = (GameStateUDP *)(buf + sizeof(PacketHeader));
      printf("[Client 1] Reçu mise à jour de l'état ! GameID: %d, Board: %s\n",
             gsu->game_id, gsu->fen_board);
    }
  }

  close(c1);
  close(c2);
  return 0;
}
