# Campo Minado (Minesweeper) Multiplayer Server

This is the **Campo Minado** (Minesweeper) multiplayer game server for the **SelectJogos** platform. It allows multiple players to connect via the Lobby Server and collaboratively play Minesweeper in real-time.

---

## Key Features

* **Multiplayer Collaboration:** Players share the same board and can simultaneously reveal cells or place flags.
* **Three Difficulty Rooms:**
  * **Room 1 (Easy):** 10x10 board with 10 mines (Max 3 players)
  * **Room 2 (Medium):** 16x16 board with 40 mines (Max 5 players)
  * **Room 3 (Hard):** 20x20 board with 80 mines (Max 8 players)
* **Penalty System:** Stepping on a mine doesn't end the game, but adds a time penalty (15s for Easy, 20s for Medium, 30s for Hard) to the team's final completion time.
* **Team Leaderboard (`!rank`):** When the board is completely cleared, the host can name the team (`!name <TeamName>`) to save their completion time (plus penalties) to the persistent leaderboard.
* **In-Game Nicknames:** Players' Lobby nicknames are synchronized directly into the match, displaying in headers and the leaderboard.

---

## Directory Structure

```text
games/minesweeper/
├── build/
│   └── game                      # Compiled game executable
├── docs/
│   ├── ARCHITECTURE.md           # Internal architecture, networking, and class design
│   └── GAME_DEVELOPMENT.md       # Step-by-step guide to game rules and TUI protocols
├── objs/                         # Compiled object files (.o)
├── src/
│   ├── lib/
│   │   ├── server.cpp            # Reusable TCP transport layer for Lobby integration
│   │   └── server.hpp
│   ├── board.cpp                 # Minesweeper logic, flood-fill, and ASCII rendering
│   ├── board.hpp
│   ├── game_actions.cpp          # Command handlers (join, play, start, rank, name)
│   ├── game_connection.cpp       # Handlers for client connections and disconnections
│   ├── game.cpp                  # Server lifecycle and command registry
│   ├── leaderboard.cpp           # File-backed stats parser and ranker
│   ├── main.cpp                  # Entry point
│   ├── player.cpp                # Player entity model
│   └── room.cpp                  # Room instance model (manages boards and states)
├── Makefile                      # Build configuration
└── README.md                     # This file
```

---

## Building and Running

### Prerequisites
* A C++17 (or newer) compatible compiler (`g++` or `clang++`)
* POSIX environment (Linux / WSL / macOS) with `pthread` support
* `make`

### 1. Build the Game Binary
From `games/minesweeper/` (or from the repository root via `make games`):

```bash
make
```
This compiles all source files in `src/` and generates the executable at `games/minesweeper/build/game`.

### 2. Running with the Lobby Server
This game is registered as `LOCAL` in `src/server/game.config`, meaning the Lobby Server automatically spawns `build/game <ephemeral_port>` whenever a room host executes `StartGame` (`sg`). You do not need to run `./build/game` manually.
