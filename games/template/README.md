# Game Server Template

The **Game Server Template** is a complete, production-ready C++17 multiplayer game server designed to integrate seamlessly with the **SelectJogos Lobby Server**. It implements the **"Higher or Lower" number-guessing game while providing a reusable TCP networking library (`src/lib/`) and an extensible command-dispatch architecture for building new games.

---

## Key Features

* **Dual Execution Mode (`LOCAL` & `REMOTE`):** Supports both on-demand process spawning by the Lobby Server (`LOCAL` mode via `fork`/`exec`) and standalone daemon execution (`REMOTE` mode) using the exact same binary.
* **Decoupled TCP Networking Layer (`src/lib/`):** Encapsulates socket creation, connection handshakes, newline-delimited token framing (`\n`), `SIGPIPE` shielding, and thread management inside the `Server` class so game developers can focus exclusively on game rules.
* **$O(1)$ Command Registry (`_commands`):** Routes incoming Lobby packets (`PlayerAction`, `ServerAction`, `PlayerDisconnected`) via an `std::unordered_map` of lambda handlers inside the `Game` class.
* **Complete Multiplayer Mechanics:** Demonstrates player registration, duplicate-move prevention, private per-player feedback (`LogChannel <ClientID>`), public match broadcasts (`LogChannel All`), host administrative resets (`ResetRound`), and mid-game player disconnection handling.

---

## Directory Structure

```text
games/template/
├── build/
│   └── game                      # Compiled game executable
├── docs/
│   ├── ARCHITECTURE.md           # Internal architecture, networking, and class design
│   └── GAME_DEVELOPMENT.md       # Step-by-step guide to creating a new game from this template
├── objs/                         # Compiled object files (.o)
├── src/
│   ├── lib/
│   │   ├── server.cpp            # Reusable TCP transport layer for Lobby integration
│   │   └── server.hpp
│   ├── config.hpp                # Match constants
│   ├── game.cpp                  # Game state machine and command handlers
│   ├── game.hpp
│   ├── main.cpp                  # Entry point and signal configuration
│   ├── player.cpp                # Player entity model
│   └── player.hpp
├── .gitignore
├── Makefile                      # Build configuration
└── README.md                     # This file
```

---

## Documentation (`docs/`)

Detailed technical documentation for game developers and maintainers is located in the [`docs/`](./docs) directory:

1. **[Template Architecture & Internal Design (`docs/ARCHITECTURE.md`)](./docs/ARCHITECTURE.md)**
   Explains the separation between `src/lib/Server` and `src/Game`, the TCP framing lifecycle, command routing, and how the Higher or Lower state machine operates.
2. **[Game Development Guide (`docs/GAME_DEVELOPMENT.md`)](./docs/GAME_DEVELOPMENT.md)**
   Provides a step-by-step tutorial on cloning this template to build a brand-new game, adding custom player and host commands, using optional `@SCREEN` TUI directives, and registering your game in `src/server/game.config`.

---

## Building and Running

### Prerequisites
* A C++17 (or newer) compatible compiler (`g++` or `clang++`)
* POSIX environment (Linux / WSL / macOS) with `pthread` support
* `make`

### 1. Build the Game Binary
From `games/template/` (or from the repository root via `make games`):

```bash
make
```
This compiles all source files in `src/` and `src/lib/` into `objs/` and generates the executable at `games/template/build/game`.

### 2. Running with the Lobby Server
* **In `LOCAL` Mode (Default):** You do **not** need to run `./build/game` manually. When registered as `LOCAL` in `src/server/game.config`, the Lobby Server automatically spawns `build/game <ephemeral_port>` whenever a room host executes `StartGame` (`sg`).
* **In `REMOTE` Mode (Standalone):** To run the game as an independent server listening on a fixed port (e.g., `9001`):
  ```bash
  make run PORT=9001
  # Or directly:
  ./build/game 9001
  ```