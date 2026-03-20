#ifndef GATEWAY_H
#define GATEWAY_H

/**
 * @file gateway.h
 * @brief Point d'entrée de la passerelle réseau (Gateway).
 * 
 * La Gateway est le seul composant exposé à l'extérieur. Elle gère les connexions
 * TCP des clients, reçoit leurs paquets, et les route vers les services internes
 * (Auth, Matchmaker, GameWorker) via des files de messages IPC.
 */

/**
 * @brief Initialise le gateway, crée les ressources IPC et lance la boucle select().
 * Cette fonction est bloquante et gère tout le cycle de vie des connexions réseaux.
 */
void start_gateway();

#endif // GATEWAY_H
