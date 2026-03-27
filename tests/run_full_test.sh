#!/bin/bash

# Chemins vers les binaires
SERVER="./server_app"
AUTH="./auth_app"
MATCHMAKER="./matchmaker_app"
GAMEWORKER="./gameworker_app"
TEST_IPC="./test_suite"
TEST_GAME="./test_game_flow"

echo "=== DÉMARRAGE DU SYSTÈME COMPLET ==="

# Nettoyage des processus existants
killall server_app auth_app matchmaker_app gameworker_app 2>/dev/null

# Compilation
echo "[1/4] Compilation..."
make all

# Démarrage des services
echo "[2/4] Lancement des services..."
$AUTH > /dev/null &
AUTH_PID=$!
$MATCHMAKER > /dev/null &
MATCH_PID=$!
$GAMEWORKER > /dev/null &
GW_PID=$!

sleep 1

$SERVER > /dev/null &
SERVER_PID=$!

# Attendre que les services soient prêts
sleep 2

echo "[3/4] Exécution de la suite de tests IPC..."
$TEST_IPC

echo "[4/4] Exécution du test de flux de jeu (Matchmaking + Moves)..."
$TEST_GAME

# Nettoyage
echo "=== ARRÊT DU SYSTÈME ==="
kill $AUTH_PID $MATCH_PID $GW_PID $SERVER_PID 2>/dev/null
echo "Terminé."
