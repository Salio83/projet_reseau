/**
 * @file serveur.c
 * @brief Version simplifiée (test) d'un serveur TCP.
 * 
 * Ce fichier est un exemple de base pour tester la connectivité TCP.
 * Pour la logique réelle du projet, voir les fichiers dans le dossier 'services/'.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {

    printf("Début programme\n");

    int dS = socket(PF_INET, SOCK_STREAM, 0);
    printf("Socket Créé\n");


    struct sockaddr_in ad;
    ad.sin_family = AF_INET;
    ad.sin_addr.s_addr = INADDR_ANY ;
    ad.sin_port = htons(atoi(argv[1])) ;
    bind(dS, (struct sockaddr*)&ad, sizeof(ad)) ;
    printf("Socket Nommé\n");

    listen(dS, 7) ;
    printf("Mode écoute\n");

    struct sockaddr_in aC ;
    socklen_t lg = sizeof(struct sockaddr_in) ;
    int dSC = accept(dS, (struct sockaddr*) &aC,&lg) ;
    printf("Client Connecté\n");

    char msg [20] ;
    recv(dSC, msg, sizeof(msg), 0) ;
    printf("Message reçu : %s\n", msg) ;

    int r = 10 ;

    send(dSC, &r, sizeof(int), 0) ;
    printf("Message Envoyé\n");
    shutdown(dSC, 2) ;
    shutdown(dS, 2) ;
    printf("Fin du programme");
}