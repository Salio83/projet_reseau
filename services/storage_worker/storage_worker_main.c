#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/file.h>

#include "../../common/ipc_utils/ipc_keys.h"
#include "../../common/ipc_utils/ipc_utils.h"
#include "../../common/network_models/packet_types.h"

#define HISTORY_FILE "games_history.log"
#define STATS_FILE "player_stats.csv"

static void append_to_history(SaveHistoryReq *req) {
    int fd = open(HISTORY_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) {
        perror("open history file");
        return;
    }
    
    // Verrouillage exclusif
    if (flock(fd, LOCK_EX) == -1) {
        perror("flock history");
        close(fd);
        return;
    }

    time_t rawtime;
    struct tm *timeinfo;
    char time_str[80];
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);

    char result_str[32];
    if (req->result == 1) strcpy(result_str, "WHITE_WINS");
    else if (req->result == 2) strcpy(result_str, "BLACK_WINS");
    else strcpy(result_str, "DRAW");

    char buffer[512];
    int len = snprintf(buffer, sizeof(buffer), "[%s] Room %u | %s (White) vs %s (Black) | Moves: %u | Result: %s\n",
                       time_str, req->room_id, req->white_name, req->black_name, req->move_count, result_str);
    
    write(fd, buffer, len);
    
    flock(fd, LOCK_UN);
    close(fd);
}

// Fonction utilitaire naïve pour mettre à jour les statistiques
// Normalement on lirait le CSV, on modifierait en mémoire, et on le réécrirait 
// de façon sécurisée via un fichier temporaire + atomic rename.
static void update_player_stat(const char* username, int is_win, int is_draw) {
    // Note: Pour un système en production massive, SQLite ou Redis serait préférable
    // car modifier un CSV in-place via C est fastidieux. 
    // Pour cet exercice, on ajoute simplement une ligne de log de performance
    int fd = open(STATS_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd < 0) return;
    if (flock(fd, LOCK_EX) == -1) { close(fd); return; }

    char buffer[256];
    const char *status = is_win ? "WIN" : (is_draw ? "DRAW" : "LOSS");
    int len = snprintf(buffer, sizeof(buffer), "User: %s | Result: %s\n", username, status);
    write(fd, buffer, len);
    
    flock(fd, LOCK_UN);
    close(fd);
}

static void handle_save_history(SaveHistoryReq *req) {
    append_to_history(req);
    
    if (req->result == 1) {
        update_player_stat(req->white_name, 1, 0);
        update_player_stat(req->black_name, 0, 0);
    } else if (req->result == 2) {
        update_player_stat(req->white_name, 0, 0);
        update_player_stat(req->black_name, 1, 0);
    } else {
        update_player_stat(req->white_name, 0, 1);
        update_player_stat(req->black_name, 0, 1);
    }
}

int main(void) {
    printf("[Storage Worker] Demarrage...\n");

    int global_mq = ipc_msg_get(ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID));
    if (global_mq == -1) {
        perror("ipc_msg_get failed");
        exit(1);
    }

    char msg_buffer[MAX_MSG_SIZE];
    while (1) {
        int nbytes = ipc_msg_receive(global_mq, msg_buffer, sizeof(msg_buffer), MSG_TYPE_STORAGE);
        if (nbytes <= 0) continue;

        PacketHeader *header = (PacketHeader *)msg_buffer;
        void *payload = msg_buffer + sizeof(PacketHeader);

        if (header->type == PACKET_SAVE_HISTORY) {
            handle_save_history((SaveHistoryReq *)payload);
        }
    }

    return 0;
}
