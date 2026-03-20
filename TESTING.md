# Guide de Test - Projet Réseau Échecs

Ce document explique comment tester les différentes fonctionnalités du projet, de la compilation à l'exécution des tests automatisés et manuels.

## 1. Prérequis

Assurez-vous d'avoir les outils suivants installés :
- `gcc` (compilateur C)
- `make`
- `ipcs` / `ipcrm` (pour la gestion des files de messages IPC)
- `raylib` (pour la partie graphique, si applicable)

## 2. Compilation

Pour compiler l'ensemble du projet (serveurs, services et outils de test) :

```bash
make all
```

Pour nettoyer les fichiers compilés :

```bash
make clean
```

## 3. Tests Automatisés

### Test Complet du Système
Un script est disponible pour lancer tous les services, exécuter une suite de tests, puis tout arrêter proprement.

```bash
make test-full
# ou
bash tests/run_full_test.sh
```

Ce script teste :
1. La compilation de tous les modules.
2. Le lancement des services (Auth, Matchmaker, GameWorker, Server).
3. La communication IPC entre les services.
4. Un flux de jeu complet (Matchmaking + déplacements de pièces).

### Test de la Suite de Fonctionnalités (IPC)
Pour tester uniquement la communication entre les files de messages :

```bash
make test
```

## 4. Tests Manuels

### Utilisation du Client Interactif
Le `client_interactive` permet de simuler un client réel se connectant au serveur.

1. **Lancer les services dans des terminaux séparés (ou via le script de test) :**
   ```bash
   ./auth_app
   ./matchmaker_app
   ./gameworker_app
   ./server_app
   ```

2. **Lancer un ou plusieurs clients interactifs :**
   ```bash
   ./client_interactive <IP_SERVEUR> <PORT>
   ```
   *(Par défaut, utilisez `127.0.0.1` et le port configuré dans le serveur)*

### Simulation de Paquets Spécifiques
L'outil `test_client` peut être utilisé pour envoyer des paquets bruts pour tester des cas limites.

```bash
./test_client
```

## 5. Dépannage et Nettoyage IPC

Le projet utilise des files de messages UNIX (System V IPC). Si un service plante, les files de messages peuvent rester actives et bloquer le redémarrage.

### Nettoyer les ressources IPC
Pour supprimer toutes les files de messages créées par votre utilisateur :

```bash
make clean-ipc
```

### Vérifier l'état des files
Pour voir les files de messages actuellement actives :

```bash
ipcs -q
```

## 6. Structure des Tests
- `tests/test_all_features.c` : Vérifie la robustesse des utilitaires IPC.
- `tests/test_game_flow.c` : Simule un cycle de vie de partie (connexion -> matchmaking -> jeu).
- `tests/interactive_client.c` : Client console interactif pour tester l'enchaînement des commandes.
