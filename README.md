# World Polytech Chess — Networked Chess Platform

A multiplayer online chess platform written in C for Linux. Players connect to a central server, get matched against each other, play in real time, chat, spectate ongoing games, take part in elimination tournaments, and browse their game history.

The server is built as a set of small independent **services** (processes) that communicate through a **System V message queue**, while clients talk to the server over **TCP** (reliable requests) and **UDP** (fast game-state updates). The graphical client is built with [raylib](https://www.raylib.com/).

## Features

- **Authentication** — connect with a username and get a session ID.
- **Matchmaking** — join a queue and get paired with another player automatically.
- **Full chess rules** — move validation, check, checkmate, stalemate, castling, en passant and promotion, enforced server-side.
- **Real-time play** — moves are sent over TCP; board updates are pushed to players and spectators over UDP.
- **Chat** — per-game chat room shared by players and spectators.
- **Spectator mode** — list active games and watch any of them live.
- **Tournaments** — create a tournament (configurable number of players), join one, and play through a single-elimination bracket.
- **Game history & stats** — finished games are logged to `games_history.log` and player statistics to `player_stats.csv`.
- **Forfeit / disconnection handling** — a player leaving mid-game loses by forfeit.

## Architecture

```
            ┌──────────────────────┐        ┌──────────────────────┐
            │  chess_gui_client    │  ...   │  client_interactive  │
            │  (raylib GUI)        │        │  (terminal client)   │
            └─────────┬────────────┘        └──────────┬───────────┘
                TCP 6767 / UDP 6768                    │
                      └──────────────┬─────────────────┘
                                     ▼
                           ┌───────────────────┐
                           │      Gateway      │  sockets ⇄ IPC bridge
                           └─────────┬─────────┘
                                     │  System V message queue
                                     │  (one queue, routed by mtype)
   ┌──────────┬──────────────┬───────┴──────┬──────────┬─────────────┐
   ▼          ▼              ▼              ▼          ▼             ▼
  Auth   Matchmaker     Game Worker       Chat    Tournament     Storage
                      (chess engine)                           (history/stats)
```

- **`server_app`** (`services/server_main.c`) is the orchestrator: it cleans up stale IPC resources, `fork`s/`exec`s every service, starts the gateway, and shuts everything down cleanly on `SIGINT`/`SIGTERM`.
- **Gateway** (`services/gateway/`) accepts TCP clients, receives UDP registrations, and forwards packets between clients and the internal services.
- **Services** each read their own message type (`MSG_TYPE_AUTH`, `MSG_TYPE_MATCHMAKING`, `MSG_TYPE_GAMEWORKER`, …) from the shared queue defined in `common/ipc_utils/ipc_keys.h`.
- **Protocol**: every packet starts with a `PacketHeader` (`type`, `length`, `session_id`) followed by a typed payload. All packet types and structures are defined in `common/network_models/packet_types.h`.

## Project Layout

```
apps/
  chess_gui_client/      Networked graphical client (raylib)
  chess_standalone/      Local standalone chess game
common/
  chess_engine/          Chess rules and game state
  render/                Board / piece rendering (raylib)
  ipc_utils/             Message queue & shared memory helpers, IPC keys
  network_models/        Network packet definitions
services/
  server_main.c          Orchestrator
  gateway/               TCP/UDP ⇄ IPC gateway
  auth/                  Authentication service
  matchmaker/            Matchmaking queue
  game_worker/           Game rooms, move validation, spectators
  chat/                  Chat rooms
  tournament/            Tournament brackets
  storage_worker/        Game history and player statistics
tests/
  interactive_client.c   Terminal client for manual testing
assets/img/              Piece sprites
```

## Requirements

- Linux (System V IPC, POSIX sockets)
- `gcc` and `make`
- [raylib](https://www.raylib.com/) for the graphical clients — either installed system-wide or as a static library at `libs/libraylib.a` (detected automatically by the Makefile)

## Build

```bash
make all
```

This produces the service binaries (`server_app`, `auth_app`, `matchmaker_app`, `gameworker_app`, `chat_app`, `tournament_app`, `storage_app`) and the clients (`chess_gui_client`, `chess_game`, `client_interactive`).

## Run

Start the server and all its services:

```bash
make run
```

Keep this terminal open; press `Ctrl+C` to stop every service and clean up IPC resources.

Then start one or more clients (e.g. two GUI clients with different usernames to play against each other):

```bash
./chess_gui_client
```

The clients connect to `127.0.0.1` on TCP port **6767** and UDP port **6768**.

### Terminal client

For quick testing without a GUI:

```bash
./client_interactive
```

| Command            | Description                     |
|--------------------|---------------------------------|
| `auth <name>`      | Log in (e.g. `auth alice`)      |
| `join`             | Enter the matchmaking queue     |
| `move <from> <to>` | Play a move (e.g. `move e2 e4`) |
| `chat <message>`   | Send a chat message             |
| `list`             | List active games               |
| `watch <id>`       | Spectate a game                 |
| `history`          | Show game history               |
| `quit`             | Exit                            |

### Troubleshooting

If services fail to restart (e.g. a message queue already exists after a crash), remove leftover IPC resources:

```bash
make clean-ipc
```

`make clean` removes all compiled binaries.

More testing details (in French) are available in [`TESTING.md`](TESTING.md).

## Contributors

- Elliot Barthélemy
- Mael Torset
- Hugo Saulig
