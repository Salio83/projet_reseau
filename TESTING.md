# 🧪 Guide de Test - World Polytech Chess

Ce document explique comment tester les différentes fonctionnalités du projet, du moteur de jeu à la partie en ligne.

## 🛠️ Préparation de l'environnement

Avant chaque session de test, assurez-vous de compiler l'intégralité du projet :

```bash
make clean
make all
```

## 🚀 Scénario 1 : Test de la partie en ligne (2 Joueurs Graphiques)

C'est le test le plus complet pour vérifier l'intégration totale.

1.  **Lancer le serveur central :**
    Dans un terminal dédié :
    ```bash
    ./server_app
    ```
    *Vous devriez voir les services (Auth, Matchmaker, GameWorker, Gateway) démarrer.*

2.  **Lancer le premier joueur (Blancs) :**
    Dans un second terminal :
    ```bash
    ./chess_game
    ```
    Cliquez sur **PLAY**. L'écran affichera "Recherche d'un adversaire...".

3.  **Lancer le second joueur (Noirs) :**
    Dans un troisième terminal :
    ```bash
    ./chess_game
    ```
    Cliquez sur **PLAY**. 
    *La partie doit se lancer instantanément sur les deux fenêtres.*

4.  **Vérification :**
    - Faites un coup avec les Blancs.
    - Vérifiez que le plateau du joueur Noir se met à jour automatiquement.
    - Vérifiez que le joueur Noir peut maintenant jouer son coup.

---

## 💻 Scénario 2 : Test Hybride (GUI vs Terminal)

Utile pour tester la compatibilité du protocole.

1.  **Lancer le serveur :** `./server_app`
2.  **Lancer le client graphique :** `./chess_game` (cliquez sur PLAY)
3.  **Lancer le client interactif :**
    ```bash
    ./client_interactive
    ```
    Dans le terminal du client interactif, tapez :
    ```text
    auth Player2
    join
    ```
    *Le match démarre. Vous pouvez jouer dans le terminal avec `move e2 e4` et voir le résultat sur la fenêtre graphique.*

---

## 🧹 Nettoyage des ressources (En cas de problème)

Si le serveur plante ou si les services ne veulent plus se lancer, les ressources IPC sont peut-être bloquées. Utilisez :

```bash
make clean-ipc
```

## 🧪 Tests Unitaires (Moteur de jeu)

Pour vérifier que les règles des échecs (échec et mat, pat, déplacements) sont respectées :

```bash
make test
```
