# Lobby Client (TUI)

The **Lobby Client** is a multi-threaded C++17 Terminal User Interface (TUI) application designed to connect to the Lobby Server, manage rooms, and play multiplayer games in a clean, split-screen console layout. Built strictly with standard POSIX headers and ANSI escape sequences.

---

## Key Features

* **Flicker-Free 5-Zone TUI Layout:** Divides the terminal into a live status **Header Bar** (`ClientID`, current room, and `[Lobby]` / `[Waiting]` / `[Playing]` state), an optional **Game Canvas**, a scrolling **Log Feed**, a persistent **Alert/Error Bar**, and a non-blocking **Input Prompt** at the bottom.
* **Automatic Protocol Abstraction:** Eliminates manual protocol formatting. The client automatically generates and tracks bounded `<MsgID>` tokens, correlates asynchronous `Response` packets to update room state, and prevents incoming server messages from corrupting user text mid-typing.
* **Case-Insensitive Command Shorthands:** Supports intuitive two-letter aliases in uppercase or lowercase for all Lobby and Game commands (e.g., `lg` for `ListGames`, `cr` for `CreateRoom`, `jr` for `JoinRoom`, `sg` for `StartGame`, `pa` for `PlayerAction`, and `sa rr` for `ServerAction ResetRound`).
* **Optional Graphical Canvas Sub-Protocol:** Intercepts visual directives (`@SCREEN`, `@LINE`, `@CLEAR`, `@ALERT`) inside `LogChannel` messages via an $O(1)$ directive registry (`_screen_directives`), allowing games to render ASCII boards and scoreboards while remaining 100% backwards-compatible with plain-text games.
* **Hot-Reloadable Help System:** Reads `help.txt` from disk whenever the user types `help` (`h` or `?`), allowing command documentation to be updated at any time without recompiling the binary.

---

## Directory Structure

```text
src/client/
├── docs/
│   ├── ARCHITECTURE.md       # Internal MVC design, threading model, and maintenance guide
│   ├── SCREEN_PROTOCOL.md    # Optional @SCREEN/@LINE/@CLEAR/@ALERT game UI specification
│   └── USER_GUIDE.md         # TUI layout, command shorthands, and help.txt customization
├── objs/                     # Compiled object files (mirrors src/ subdirectories)
├── src/
│   ├── core/                 # Thread-safe ClientState model and ScreenSnapshot
│   ├── network/              # Async TCP NetworkClient and ProtocolParser controller
│   ├── ui/                   # POSIX termios raw-mode controller and ANSI TerminalUI renderer
│   └── main.cpp              # Entry point, signal shielding, and component wiring
├── help.txt                  # External hot-reloadable help menu
├── Makefile                  # Build configuration
└── README.md                 # This file
```

---

## Documentation (`docs/`)

Detailed technical documentation for users, game developers, and maintainers is organized into three manuals inside the [`docs/`](./docs) directory:

1. **[User Guide & Command Reference (`docs/USER_GUIDE.md`)](./docs/USER_GUIDE.md)**
   Explains the 5 visual zones of the TUI screen, automatic `<MsgID>` handling, the complete table of commands and case-insensitive shorthands, a two-player match walkthrough, and how to customize `help.txt`.
2. **[Game Screen Sub-Protocol Specification (`docs/SCREEN_PROTOCOL.md`)](./docs/SCREEN_PROTOCOL.md)**
   Specifies how games can optionally render persistent ASCII boards, status panels, and warning banners inside the Client TUI using `@SCREEN`, `@LINE`, `@CLEAR`, and `@ALERT` directives over `LogChannel`.
3. **[Client Architecture & Maintenance Guide (`docs/ARCHITECTURE.md`)](./docs/ARCHITECTURE.md)**
   Details the internal Reactive MVC architecture, the two-thread concurrency model, mutex lock-ordering invariants, `<termios.h>` raw mode and Alternate Screen Buffer management, UTF-8 safety, and step-by-step instructions for registering new aliases or screen directives.

---

## Building and Running

### Prerequisites
* A C++17 (or newer) compatible compiler (`g++` or `clang++`)
* POSIX environment (Linux / WSL / macOS) with `pthread` support
* `make`

### 1. Build the Client
You can compile the client either from the unified `src/Makefile` or directly inside `src/client/`:

```bash
# Option A: From src/ (builds both Server and Client, or only Client)
make
make client

# Option B: From src/client/
make
```

This compiles all source files in `src/core/`, `src/network/`, and `src/ui/` into `objs/` and produces the `client` executable in `src/client/`.

### 2. Run the Client

#### Using the Unified `src/Makefile` (from `src/`)
```bash
# Start Lobby Server in background + Launch TUI Client in foreground
make run PORT=8080

# Launch only the TUI Client (connecting to an already running server)
make run-client HOST=127.0.0.1 PORT=8080 HELP=help.txt
```

#### Using `src/client/Makefile` or Direct Binary (from `src/client/`)
```bash
# Run via local Makefile
make run HOST=127.0.0.1 PORT=8080 HELP=help.txt

# Direct execution
./client <LOBBY_IP> <LOBBY_PORT> [HELP_FILE]

# Example using default help.txt
./client 127.0.0.1 8080
```