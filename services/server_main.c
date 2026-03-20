#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "gateway/gateway.h"

/**
 * @file server_main.c
 * @brief Point d'entrée principal pour lancer la Gateway.
 * 
 * Ce fichier permet de démarrer le serveur de manière isolée ou globale.
 * Actuellement, il appelle directement la fonction start_gateway() qui gère
 * tout l'orchestre réseau et IPC.
 */

int main() {
    printf("Démarrage du serveur World Polytech Chess (Processus Unique)...\n");

    // Lancement de la boucle principale du Gateway.
    // Ce programme doit idéalement être lancé en premier.
    start_gateway();

    return 0;
}
