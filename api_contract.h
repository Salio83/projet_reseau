#include <stdint.h>

// Codes des types de messages
#define REQ_LOGIN 1
#define RES_LOGIN_OK 2
#define RES_LOGIN_FAIL 3

#define REQ_MATCHMAKING 10
#define RES_MATCH_FOUND 11

#define REQ_MOVE 20
#define RES_MOVE_OK 21
#define RES_MOVE_INVALID 22

// En-tête envoyé avant chaque message
typedef struct {
  uint16_t type;   // l'un des codes définis ci-dessus
  uint16_t length; // taille de la charge utile qui suit
} msg_header_t;

// Charge utile pour REQ_MOVE
typedef struct {
  int32_t room_id;
  char from[3];
  char to[3];
} req_move_t;

typedef struct {
  int32_t room_id;
  char from[3];
  char to[3];
} res_move_ok_t;

typedef struct {
  char reason[64]; // why the move was rejected
} res_move_invalid_t;