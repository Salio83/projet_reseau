#!/bin/bash

# Script pour lancer les 4 services en arrière-plan

set -e

cd "$(dirname "$0")"

echo "Lancement des services..."
echo ""

# Lancer les 4 services en arrière-plan
./auth_app &
AUTH_PID=$!
echo "Auth Service lancé (PID: $AUTH_PID)"

./matchmaker_app &
MATCHMAKER_PID=$!
echo "Matchmaker Service lancé (PID: $MATCHMAKER_PID)"

./gameworker_app &
GAMEWORKER_PID=$!
echo "GameWorker Service lancé (PID: $GAMEWORKER_PID)"

./server_app &
SERVER_PID=$!
echo "Server Service lancé (PID: $SERVER_PID)"

echo ""
echo "=========================================="
echo "Tous les services sont lancés !"
echo "=========================================="
echo ""
echo "PIDs des services :"
echo "  Auth:       $AUTH_PID"
echo "  Matchmaker: $MATCHMAKER_PID"
echo "  GameWorker: $GAMEWORKER_PID"
echo "  Server:     $SERVER_PID"
echo ""
echo "Pour arrêter les services, appuyez sur Ctrl+C"
echo ""

# Trap pour arrêter les services proprement avec Ctrl+C
cleanup() {
    echo ""
    echo "Arrêt des services..."
    kill $AUTH_PID $MATCHMAKER_PID $GAMEWORKER_PID $SERVER_PID 2>/dev/null || true
    echo "Services arrêtés."
    exit 0
}

trap cleanup SIGINT SIGTERM

# Garder le script actif et surveiller les services
wait
