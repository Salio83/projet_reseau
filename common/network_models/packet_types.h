#ifndef PACKET_TYPES_H
#define PACKET_TYPES_H

#include <stdint.h>

#define MAX_USERNAME_LEN 32
#define MAX_CHAT_MESSAGE_LEN 256
#define MAX_ERROR_MESSAGE_LEN 128
#define MAX_FEN_BOARD_LEN 90
#define MAX_ACTIVE_GAMES_LISTED 16

typedef enum {
    PACKET_AUTH_REQ = 1,
    PACKET_AUTH_OK = 2,
    PACKET_MATCHMAKING_REQ = 3,
    PACKET_PLAYER_MOVE = 4,
    PACKET_CHAT_MSG = 5,
    PACKET_UDP_REGISTER_REQ = 6,
    PACKET_LIST_ACTIVE_GAMES_REQ = 7,
    PACKET_LIST_ACTIVE_GAMES_RESP = 8,
    PACKET_SPECTATE_JOIN_REQ = 9,
    PACKET_SPECTATE_JOIN_OK = 10,
    PACKET_SPECTATE_LEAVE_REQ = 11,
    PACKET_SPECTATE_LEAVE_OK = 12,
    PACKET_CLIENT_DISCONNECTED = 13,

    PACKET_GAME_STARTED = 20,
    PACKET_GAME_SNAPSHOT = 21,
    PACKET_CHAT_BROADCAST = 22,
    PACKET_ERROR = 23,
    PACKET_MOVE_ERROR = 24,

    PACKET_GAME_UPDATE_UDP = 50
} PacketType;

typedef enum {
    ROOM_ROLE_NONE = 0,
    ROOM_ROLE_PLAYER = 1,
    ROOM_ROLE_SPECTATOR = 2
} RoomRole;

typedef struct {
    uint16_t type;
    uint16_t length;
    uint32_t session_id;
} PacketHeader;

typedef struct {
    char username[MAX_USERNAME_LEN];
    char password_hash[64];
} AuthRequest;

typedef struct {
    uint32_t session_id;
    char username[MAX_USERNAME_LEN];
} AuthOk;

typedef struct {
    uint32_t player_id;
    uint16_t current_elo;
    char username[MAX_USERNAME_LEN];
} MatchmakingRequest;

typedef struct {
    uint32_t room_id;
    uint32_t opponent_session_id;
    uint8_t your_color;
    char opponent_username[MAX_USERNAME_LEN];
    char white_username[MAX_USERNAME_LEN];
    char black_username[MAX_USERNAME_LEN];
} GameStarted;

typedef struct {
    uint32_t game_id;
    char from_square[3];
    char to_square[3];
    char promotion;
} PlayerMove;

typedef struct {
    uint32_t room_id;
    char message[MAX_CHAT_MESSAGE_LEN];
} ChatMessage;

typedef struct {
    uint32_t session_id;
} UdpRegisterRequest;

typedef struct {
    uint32_t room_id;
    char username[MAX_USERNAME_LEN];
} SpectateJoinRequest;

typedef struct {
    uint32_t room_id;
} SpectateLeaveRequest;

typedef struct {
    uint32_t room_id;
} SpectateStatus;

typedef struct {
    uint32_t room_id;
    char white_username[MAX_USERNAME_LEN];
    char black_username[MAX_USERNAME_LEN];
    uint16_t spectator_count;
    uint16_t move_count;
    uint8_t current_turn;
    uint8_t active;
} ActiveGameInfo;

typedef struct {
    uint16_t game_count;
    uint8_t truncated;
    ActiveGameInfo games[MAX_ACTIVE_GAMES_LISTED];
} ActiveGamesResponse;

typedef struct {
    uint32_t room_id;
    char fen_board[MAX_FEN_BOARD_LEN];
    uint16_t move_count;
    uint8_t current_turn;
    char white_username[MAX_USERNAME_LEN];
    char black_username[MAX_USERNAME_LEN];
} GameSnapshot;

typedef struct {
    uint16_t code;
    char message[MAX_ERROR_MESSAGE_LEN];
} PacketError;

typedef struct {
    uint32_t room_id;
    char author_name[MAX_USERNAME_LEN];
    uint32_t author_session_id;
    uint64_t timestamp_ms;
    char message[MAX_CHAT_MESSAGE_LEN];
} ChatBroadcast;

typedef struct {
    uint32_t room_id;
    uint8_t role;
} ClientDisconnected;

typedef struct {
    uint32_t room_id;
    char fen_board[MAX_FEN_BOARD_LEN];
    uint16_t move_count;
    uint8_t current_turn;
    char from_square[3];
    char to_square[3];
    char promotion;
    uint32_t sequence_no;
} GameUpdateUDP;

#define PACKET_GAME_STATE_UDP PACKET_GAME_UPDATE_UDP
#define PACKET_SIZE(payload_type) (sizeof(PacketHeader) + sizeof(payload_type))

#endif // PACKET_TYPES_H
