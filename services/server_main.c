#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "gateway/gateway.h"

int main() {
    printf("Démarrage du serveur World Polytech Chess (Processus Unique)...\n");

    // Initialisation du Gateway (Réseau)
    // Dans une version plus complexe, nous pourrions lancer le gateway dans un thread séparé
    // ou utiliser une boucle d'événements unique (select/epoll).
    
    // Pour l'instant, on appelle directement la boucle du gateway.
    start_gateway();

    return 0;
}
