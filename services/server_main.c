#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "gateway.h"
#include "ipc_utils.h"
#include "ipc_keys.h"

#define NUM_SERVICES 7

pid_t child_pids[NUM_SERVICES];
const char* service_names[NUM_SERVICES] = {
    "auth_app",
    "matchmaker_app",
    "gameworker_app",
    "chat_app",
    "tournament_app",
    "storage_app",
    "gateway_process"
};

void cleanup_and_exit(int sig) {
    printf("\n[Server Main] Signal %d reçu. Nettoyage en cours...\n", sig);

    for (int i = 0; i < NUM_SERVICES; i++) {
        if (child_pids[i] > 0) {
            printf("[Server Main] Arrêt de %s (PID: %d)...\n", service_names[i], child_pids[i]);
            kill(child_pids[i], SIGTERM);
        }
    }

    // Attendre que tous les enfants se terminent pour éviter les zombies
    for (int i = 0; i < NUM_SERVICES; i++) {
        if (child_pids[i] > 0) {
            waitpid(child_pids[i], NULL, 0);
        }
    }

    // Nettoyage de la file de messages IPC
    key_t key = ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID);
    int msqid = msgget(key, 0666);
    if (msqid != -1) {
        printf("[Server Main] Suppression de la file de messages IPC...\n");
        ipc_msg_delete(msqid);
    }

    printf("[Server Main] Système arrêté proprement.\n");
    exit(0);
}

void launch_service(int index, const char* path) {
    pid_t pid = fork();
    if (pid == 0) {
        // Processus enfant
        printf("[Server Main] Lancement de %s...\n", path);
        if (index == 6) {
            // Le gateway est une fonction, pas un binaire séparé (actuellement)
            start_gateway();
            exit(0);
        } else {
            execl(path, path, (char *)NULL);
            perror("execl failed");
            exit(1);
        }
    } else if (pid > 0) {
        child_pids[index] = pid;
    } else {
        perror("fork failed");
        exit(1);
    }
}

int main() {
    printf("Démarrage de l'orchestrateur World Polytech Chess...\n");

    // Configuration des gestionnaires de signaux
    signal(SIGINT, cleanup_and_exit);
    signal(SIGTERM, cleanup_and_exit);

    // --- Nettoyage préliminaire ---
    printf("[Server Main] Nettoyage des ressources existantes...\n");
    
    // On tue les anciennes instances au cas où
    system("pkill -TERM auth_app > /dev/null 2>&1");
    system("pkill -TERM matchmaker_app > /dev/null 2>&1");
    system("pkill -TERM gameworker_app > /dev/null 2>&1");
    system("pkill -TERM chat_app > /dev/null 2>&1");
    system("pkill -TERM tournament_app > /dev/null 2>&1");
    system("pkill -TERM storage_app > /dev/null 2>&1");
    
    // Création préventive du fichier pour ftok s'il n'existe pas
    int fd = open(GLOBAL_MSG_QUEUE_PATH, O_CREAT | O_RDWR, 0666);
    if (fd != -1) close(fd);

    key_t key = ipc_get_key(GLOBAL_MSG_QUEUE_PATH, GLOBAL_MSG_QUEUE_ID);
    int old_msqid = msgget(key, 0666);
    if (old_msqid != -1) {
        ipc_msg_delete(old_msqid);
    }
    // Optionnel: on pourrait aussi tuer les processus orphelins via system("killall ...") 
    // mais restons-en au nettoyage IPC pour l'instant pour être prudent.

    // Initialisation du tableau des PIDs
    for (int i = 0; i < NUM_SERVICES; i++) child_pids[i] = 0;

    // Lancement des services
    launch_service(0, "./auth_app");
    launch_service(1, "./matchmaker_app");
    launch_service(2, "./gameworker_app");
    launch_service(3, "./chat_app");
    launch_service(4, "./tournament_app");
    launch_service(5, "./storage_app");
    
    // Petite pause pour laisser les services s'initialiser
    sleep(1);
    
    launch_service(6, "gateway_process");

    printf("[Server Main] Tous les services sont lancés. Appuyez sur Ctrl+C pour arrêter le serveur.\n");

    // Attendre indéfiniment (ou jusqu'à un signal)
    while (1) {
        pause();
    }

    return 0;
}
