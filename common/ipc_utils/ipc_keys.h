#ifndef IPC_KEYS_H
#define IPC_KEYS_H

#include <sys/ipc.h>
#include <sys/types.h>

#ifndef PROJECT_DIR
#define PROJECT_DIR "."
#endif

// IDs pour ftok
#define GATEWAY_PROJ_ID    'G'
#define AUTH_PROJ_ID       'A'
#define MATCHMAKING_PROJ_ID 'M'
#define GAME_WORKER_PROJ_ID 'W'
#define CHAT_PROJ_ID       'C'
#define DB_PROJ_ID         'D'
#define TOURNAMENT_PROJ_ID 'T'

// Clés générées à l'exécution avec ftok
// (Ces macros sont des helpers pour obtenir les clés)

#define GET_GATEWAY_KEY()    ftok(PROJECT_DIR, GATEWAY_PROJ_ID)
#define GET_AUTH_KEY()       ftok(PROJECT_DIR, AUTH_PROJ_ID)
#define GET_MATCHMAKING_KEY() ftok(PROJECT_DIR, MATCHMAKING_PROJ_ID)
#define GET_GAME_WORKER_KEY() ftok(PROJECT_DIR, GAME_WORKER_PROJ_ID)
#define GET_CHAT_KEY()       ftok(PROJECT_DIR, CHAT_PROJ_ID)
#define GET_DB_KEY()         ftok(PROJECT_DIR, DB_PROJ_ID)
#define GET_TOURNAMENT_KEY() ftok(PROJECT_DIR, TOURNAMENT_PROJ_ID)

#endif // IPC_KEYS_H
