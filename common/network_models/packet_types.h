#ifndef PACKET_TYPES_H
#define PACKET_TYPES_H

#include <stdint.h>

/*Enumération des types de paquets 
 Permet au Gateway (et aux autres modules) de savoir comment lire le buffer.
*/
typedef enum {
    // Requêtes TCP (Joueurs -> Serveur)
    PACKET_AUTH_REQ = 1,
    PACKET_MATCHMAKING_REQ = 2,
    PACKET_PLAYER_MOVE = 3,
    PACKET_CHAT_MSG = 4,

    // Mises à jour UDP (Serveur -> Spectateurs)
    PACKET_GAME_STATE_UDP = 50
} PacketType;

//Header
typedef struct {
    uint16_t type;       // Correspond à l'enum PacketType
    uint16_t length;     // Taille totale du paquet (utile pour TCP)
    uint32_t client_id;  // Identifiant unique du joueur ou de la session
} PacketHeader;

//Structures TCP

// Utilisé pour l'authentification 
typedef struct {
    char username[32];
    char password_hash[64]; //faut hasher le mdp
} AuthRequest;

//file d'attente
typedef struct {
    uint32_t player_id;
    uint16_t current_elo; //on fait un élo ?
} MatchmakingRequest;

// Les coups envoyés par les joueurs
typedef struct {
    uint32_t game_id;       // L'ID de la partie en cours
    char from_square[3];    // ex: "e2" (2 caractères + caractère de fin '\0')
    char to_square[3];      // ex: "e4"
    char promotion;         // 'q' (reine), 'r' (tour), etc. ou '\0' si pas de promotion
} PlayerMove;

// messagerie textuelle
typedef struct {
    uint32_t room_id;
    char message[256];
} ChatMessage;

// Structure UDP

// Envoyé en boucle (ex: 10 fois par seconde)
typedef struct {
    uint32_t game_id;
    char fen_board[90];  
    uint32_t white_time_ms; 
    uint32_t black_time_ms; 
} GameStateUDP;


//pour calculer la taille totale d'un paquet à envoyer

#define PACKET_SIZE(payload_type) (sizeof(PacketHeader) + sizeof(payload_type))

#endif // PACKET_TYPES_H