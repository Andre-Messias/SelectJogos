# Lobby Server

The **Lobby Server** is a multi-threaded C++ TCP reverse proxy and room orchestrator designed for turn-based and real-time multiplayer games. It decouples connection management, player authentication, and room lifecycle from the underlying game logic, allowing games to be developed as independent executables in any programming language.

---

## Key Features

* **Language-Agnostic Game Bridge:** Communicates with game servers over a simple newline-delimited (`\n`) plain-text TCP protocol (`GameBridge`), supporting games written in C++, Python, Rust, Go, Node.js, or any language with TCP socket support.
* **Dual Execution Modes (`LOCAL` & `REMOTE`):**
  * **`LOCAL` Mode:** Automatically allocates a free local port, spawns a dedicated child OS process per room when a match starts, isolates file descriptors, and terminates the process (`SIGTERM` $\rightarrow$ `SIGKILL`) when the match ends.
  * **`REMOTE` Mode:** Connects rooms to externally hosted, persistent game servers over IP/Port.
* **Zero-Trust Anti-Spoofing Security:** Clients never send their own `ClientID` in command payloads. The Lobby authenticates players by their TCP socket, injects the verified `EffectiveClientID` into forwarded commands, and isolates client `<MsgID>` tokens to prevent inter-player collisions.
* **Complete Room & Match Lifecycle:**
  * Public and password-protected (`[Private]`) rooms.
  * Automatic host migration if the room creator leaves.
  * Administrative host commands (`StartGame`, `StopGame`, `KickPlayer`, and `ServerAction` as `ClientID 0`).
  * Automatic player session tracking (`ConnectClient`, `DisconnectClient`, and `ReconnectClient`) and targeted event routing (`LogChannel` broadcast or private `IdList`).

---

## Directory Structure

```text
src/server/
├── docs/
│   ├── ARCHITECTURE.md       # Internal design, threading model, and maintenance guide
│   ├── CLIENT_PROTOCOL.md    # Client <-> Lobby TCP protocol specification
│   └── GAME_INTEGRATION.md   # Lobby <-> Game TCP protocol and integration manual
├── objs/                     # Compiled object files
├── src/                      # Lobby C++ source (.cpp) and header (.hpp) files
├── game.config               # Game catalog configuration (LOCAL and REMOTE entries)
├── Makefile                  # Build configuration
└── README.md                 # This file
```

---

## Documentation (`docs/`)

Detailed technical specifications are organized into three manuals inside the [`docs/`](./docs) directory:

1. **[Client Protocol Manual (`docs/CLIENT_PROTOCOL.md`)](./docs/CLIENT_PROTOCOL.md)**
   Specifies the communication protocol between client applications (or terminal tools like `nc`) and the Lobby server. Covers the client state machine, message framing, room management commands (`ListGames`, `ListRooms`, `CreateRoom`, `JoinRoom`, `LeaveRoom`), host commands (`StartGame`, `StopGame`, `ServerAction`, `KickPlayer`), and in-game command passthrough.
2. **[Game Integration Manual (`docs/GAME_INTEGRATION.md`)](./docs/GAME_INTEGRATION.md)**
   Explains how to develop and register new games in `game.config`. Details the `LOCAL` and `REMOTE` execution modes, startup readiness requirements, inbound commands (`ConnectClient`, `DisconnectClient`, `ReconnectClient`, player actions, and `Client 0` admin actions), and outbound demultiplexing (`Response` unicast vs. `LogChannel` broadcast and `IdList` routing).
3. **[Architecture & Maintenance Guide (`docs/ARCHITECTURE.md`)](./docs/ARCHITECTURE.md)**
   Intended for developers maintaining or extending the Lobby codebase. Documents the internal module breakdown, the thread-per-connection model, the mutex lock hierarchy, atomic invariants, child process isolation, `SIGPIPE` immunity, and step-by-step instructions for adding new Lobby commands.

---

## Building and Running

### Prerequisites
* A C++17 (or newer) compatible compiler (`g++` or `clang++`)
* POSIX environment (Linux / WSL) with `pthread` support
* `make`

### 1. Build the Server
From the `src/server/` directory, run:

```bash
make
```

This compiles the source files in `src/` and generates the `lobby` executable in `src/server/`.

### 2. Configure Available Games (`game.config`)
Register your games in `game.config` before starting the server:

```ini
# LOCAL Format:  <GameName>       LOCAL     <ExecutablePath>
HigherLower      LOCAL     ../../games/template/build/game

# REMOTE Format: <GameName>       REMOTE    <IPAddress>     <Port>
HigherLowerExt   REMOTE    127.0.0.1       8081
```

### 3. Start the Lobby Server
Run the binary passing the listening TCP port and an optional path to the configuration file (defaults to `game.config`):

```bash
./lobby <LOBBY_PORT> [CONFIG_FILE]
```

**Example:**
```bash
./lobby 8080 game.config
```

### 4. Quick Manual Test with Netcat (`nc`)
Once the server is running, you can connect from any terminal:

```bash
nc 127.0.0.1 8080
```

```text
LogChannel 0 "Connected with ClientID 482910"
ListGames m1
Response m1 Success | HigherLower[Local] | HigherLowerExt[Remote]
CreateRoom m2 Arena1 HigherLower
Response m2 Success 504123
StartGame m3
Response m3 Success
LogChannel 0 "Game HigherLower started in room Arena1"
```

---

## Automated End-to-End Tests

An automated Bash test suite using named pipes (FIFOs) and `nc` is located in the root `test/` directory. It validates both `LOCAL` and `REMOTE` modes, password protection, maximum player limits, duplicate move prevention, `<MsgID>` collision isolation, `ServerAction`, disconnection/reconnection, and host migration.

From the project root directory, run:

```bash
./test/run_all.sh
```