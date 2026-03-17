#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "packet_types.h"
#include "ipc_utils.h"
#include "ipc_keys.h"

int main() {
    int db_msqid = ipc_msg_get(GET_DB_KEY());

    printf("DB Gateway Service en ligne...\n");

    char buffer[MAX_MSG_SIZE];
    while (1) {
        int nbytes = ipc_msg_receive(db_msqid, buffer, MAX_MSG_SIZE, 0);
        if (nbytes > 0) {
            printf("Requête DB reçue !\n");
            // Simulation de base de données
        }
    }

    return 0;
}
