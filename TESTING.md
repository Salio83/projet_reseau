# Guide de Test - Projet Réseau Échecs

Ce guide explique comment compiler, lancer et tester rapidement le projet.

## 1. Compilation

Avant de tester, compilez tous les modules (services et clients) :

```bash
make all
```

## 2. Lancement des Services

Pour que le système fonctionne, tous les services backend doivent être actifs :

```bash
make run
```
*Laissez ce terminal ouvert. Appuyez sur `Ctrl+C` pour arrêter proprement tous les services et nettoyer les ressources IPC.*

## 3. Tests en Terminal (Client Interactif)

Pour tester rapidement la communication et les fonctionnalités sans interface graphique, utilisez le client interactif :

```bash
./client_interactive
```

### Commandes utiles dans le client :
- `auth <nom>` : Se connecter (ex: `auth alice`)
- `join` : Entrer dans la file d'attente pour une partie
- `move <de> <vers>` : Jouer un coup (ex: `move e2 e4`)
- `chat <message>` : Envoyer un message dans le salon
- `list` : Voir les parties en cours
- `watch <id>` : Observer une partie
- `history` : Voir l'historique des parties
- `quit` : Quitter le client

## 4. Tests avec Interface Graphique (GUI)

Pour une expérience réelle, lancez une ou plusieurs instances du client graphique :

```bash
./chess_gui_client
```

*Note : Vous pouvez lancer deux instances de `./chess_gui_client` et vous connecter avec des noms différents pour jouer l'un contre l'autre.*

## 5. Dépannage

Si les services ne redémarrent pas correctement (erreur "Message queue already exists" ou similaire), nettoyez manuellement les ressources IPC :

```bash
make clean-ipc
```
