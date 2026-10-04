# SelectJogos — Distributed Multiplayer Game Lobby Platform over TCP Sockets

**SelectJogos** is a distributed **Client-Server** multiplayer gaming platform written in standard **C++17** (compiled with `gcc`/`g++`), built strictly on native **POSIX Sockets (`<sys/socket.h>`)** and multi-threaded concurrency (**`std::thread` / `pthread`**) with zero external third-party library dependencies.

The ecosystem consists of three decoupled, network-connected modules:
1. **Lobby Server (`src/server/`):** A concurrent central server that authenticates client connections, manages multiple simultaneous game rooms, enforces room creator (*Host*) permissions, and acts as an asynchronous TCP reverse proxy between players and game servers (supporting both dynamic local child processes via `fork`/`exec` and remote standalone game servers).
2. **Lobby Client TUI (`src/client/`):** A flicker-free Terminal User Interface built with `<termios.h>` and ANSI escape sequences, featuring a 5-zone dynamic layout (Status Header Bar, Graphical Game Canvas, Rolling Log Feed, Persistent Alert Bar, and Non-Blocking Input Prompt).
3. **Game Server Template — Higher or Lower (`games/template/`):** A standalone multiplayer TCP game server integrated with the Lobby, demonstrating match lifecycles, private/broadcast messaging, and real-time host administrative actions.

---

## 1. Group Members

* **Member 1:** `André Luiz Santos Messias 15493857`
* **Member 2:** `Alexandre Brenner Weber 15436911`
* **Member 3:** `Matheus Marchi Baron 14598431`
* **Member 4:** `Pedro Dorigatti Aureo Ferreira 15483592`
* **Member 5:** `Pedro Henrique Vieira de Freitas 15652829`

---

## 2. Development & Build Environment

This project was developed, compiled, and tested under the following Linux environment:

* **Operating System (Linux):** Ubuntu 22.04 / 24.04 LTS (Linux Kernel 6.x / WSL2)
  *(Verify on target machine via: `uname -srm` and `lsb_release -ds`)*
* **Compiler:** GCC / G++ (`g++ (Ubuntu) 11.4.0` / `13.2.0` or newer) using `-std=c++17`
  *(Verify on target machine via: `g++ --version`)*
* **Compilation Flags:** `-Wall -Wextra -std=c++17 -pthread`
* **External Libraries:** **None** (strictly standard C++17 and POSIX headers: `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`, `<unistd.h>`, `<termios.h>`, `<sys/ioctl.h>`, `<sys/wait.h>`, `<csignal>`).

---

## 3. Socket Connection Failure & Data Transmission Verification

To guarantee fault tolerance against network drops, malformed packets, or abrupt disconnections without crashing the server or corrupting active rooms, the transport layer (`NetworkUtils`, `NetworkInterface`, `GameBridge`, and `NetworkClient`) implements the following checks:

### 3.1. Socket Initialization, Binding, and Immediate Port Reuse (`SO_REUSEADDR`)
* **System Call Verification:** Every call to `socket(AF_INET, SOCK_STREAM, 0)`, `inet_pton()`, `bind()`, `listen()`, and `connect()` is checked for negative return codes (`< 0`). On failure, descriptors are immediately closed via `close(fd)` to prevent file descriptor leaks.
* **Preventing `Address already in use` (`EADDRINUSE`):** Server sockets enable `setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))` prior to `bind()`, allowing the Lobby Server to restart immediately even if previous TCP connections remain in the kernel's `TIME_WAIT` state.
* **Thread-Safe Port Allocation & FD Isolation for Local Games:** When spawning a `LOCAL` game process, `NetworkUtils` allocates an available TCP port in the range `[10000, 65534]`. Inside the `fork()` child branch, all inherited file descriptors from `3` to `1023` are closed before calling `execl()`, preventing the child game process from holding open the Lobby's listening socket or client descriptors.

### 3.2. Broken Pipe Protection (`SIGPIPE` & `MSG_NOSIGNAL`)
* **Failure Scenario:** In POSIX systems, writing to a TCP socket whose remote peer has already closed the connection triggers a `SIGPIPE` signal, which terminates the entire process by default.
* **Dual Protection:**
  1. All entry points (`main.cpp` in Server, Client, and Game) ignore the signal globally at startup via `std::signal(SIGPIPE, SIG_IGN)`.
  2. Every outbound `send()` call passes the `MSG_NOSIGNAL` flag, converting broken-pipe writes into safe `-1` return values (`errno = EPIPE` or `ECONNRESET`) that are handled gracefully per connection thread.

### 3.3. Complete Data Transmission Guarantees (Partial Sends, `EINTR`, and Write Serialization)
* **Failure Scenario:** Because TCP is a byte-stream protocol, a single `send()` call may transmit only a fraction of the payload if the kernel socket buffer is near capacity, or may be interrupted mid-call by an OS signal (`errno == EINTR`).
* **Verification Loop:** `NetworkUtils::SendMessage` and `NetworkClient::Send` execute a strict delivery loop (`while (total_sent < length)`), advancing the buffer pointer until 100% of the payload bytes are acknowledged by the kernel:
  * If `send()` returns `< 0` with `errno == EINTR`, the transmission retries automatically.
  * If `send()` returns `< 0` (any other error) or `0`, the method aborts (`return false`) and triggers connection cleanup.
  * Concurrent outbound writes on the same socket are serialized via mutexes (`_bridge_mutex` / `_send_mutex`), preventing interleaved bytes across threads.

### 3.4. Stream Framing, Sanitization, and Buffer Overflow Protection (`MAX_BUFFER_SIZE`)
* **Failure Scenario:** TCP stream reads (`read()`) can coalesce multiple newline-delimited commands into a single read or split one command across multiple reads. Furthermore, a faulty or malicious peer could stream endless bytes without sending a `\n` delimiter.
* **Token Framing (`ReadToken`):** Incoming bytes are appended to a persistent per-connection buffer and sliced strictly on `\n` boundaries, stripping trailing `\r` characters for Telnet/`\r\n` compatibility.
* **Memory Exhaustion Guard (`MAX_BUFFER_SIZE = 8192`):** If a connection buffer reaches **8 KB** without encountering a `\n` delimiter, `ReadToken` clears the buffer and aborts the read loop, protecting the server and client from Denial-of-Service (*DoS*) memory exhaustion.

### 3.5. Abrupt Disconnection Detection (`EOF`) and Cascading Cleanup
* **Verification:** When `read()` returns `0` (*End-Of-File*, indicating the remote peer closed the socket) or an unrecoverable error, the listener loop terminates and triggers deterministic cleanup:
  * **Client Disconnection:** The Lobby closes the descriptor, removes the player from their current room, notifies any running game via `DisconnectClient internal_disc <ClientID>`, and—if the disconnected player was the room creator (*Host*)—automatically migrates host privileges to the next player in the room (or destroys the room if empty).
  * **Game Process Disconnection & Two-Stage Termination:** When a match ends or drops its connection, `GameBridge` shuts down the socket (`shutdown(fd, SHUT_RDWR)` + `close(fd)`). For `LOCAL` games, `StopLocalProcess` sends `SIGTERM` and polls `waitpid(pid, &status, WNOHANG)` for up to **200ms** (10 attempts $\times$ 20ms), escalating to `SIGKILL` if the process is unresponsive, ensuring zero zombie processes.
  * **Lobby Server Disconnection:** The Client's background `ListenLoop` catches the `EOF` and fires the disconnect callback, displaying `[!] Connection lost with Lobby server.` in the TUI red alert bar without freezing the user's terminal.

### 3.6. Readiness Polling on Local Game Startup
* When launching a `LOCAL` game binary, `GameBridge::Connect` polls `connect()` for up to **1 second** (20 attempts at 50ms intervals) outside the room mutex (`_is_starting`). If the game binary fails to boot or bind within this window, `GameBridge` cleans up the child process and returns `Response <MsgID> Fail "Failed to start or connect to game"` to the Host without blocking other rooms.

---

## 4. Concurrent Architecture & Thread Management

The platform handles multiple simultaneous connections through a structured multi-threaded model (`std::thread`, `std::mutex`, `std::atomic`):

1. **Lobby Server (`src/server/`):**
   * **Main Acceptor Thread:** Runs the `accept()` loop for incoming TCP clients, assigns unique `ClientID`s, and spawns per-client worker threads.
   * **Dedicated Per-Client Threads (`NetworkInterface::HandleClient`):** Each connected client runs on an isolated thread that parses and dispatches Lobby commands or forwards in-game actions to `GameRoom`.
   * **Dedicated Per-Match Listener Threads (`GameRoom::ListenToGame`):** Every active match spawns a dedicated thread holding a `std::shared_ptr<GameRoom>` (`shared_from_this()`) to demultiplex asynchronous `Response` and `LogChannel` packets from the Game Server back to room members.
   * **Lock Hierarchy:** Enforces a strict top-down lock order (`_clients_mutex` $\rightarrow$ `_manager_mutex` $\rightarrow$ `_room_mutex` $\rightarrow$ `_bridge_mutex` / leaf locks) combined with lock-free atomics (`_creator_id`, `_player_count`, `_is_starting`) to prevent deadlocks and contention.
2. **Lobby Client TUI (`src/client/`):**
   * **Main / UI Thread:** Manages POSIX `<termios.h>` raw mode, captures keystrokes from `STDIN_FILENO`, and renders the 5-zone layout into the ANSI Alternate Screen Buffer from immutable `ScreenSnapshot` copies.
   * **Network Reader Thread (`NetworkClient::_reader_thread`):** Continuously reads newline-delimited frames from the Lobby socket in the background, updates `ClientState`, and triggers atomic screen refreshes without interrupting or overwriting text currently being typed by the user.

---

## 5. Directory Structure & Documentation

```text
SelectJogos/
├── games/
│   └── template/                 # Template multiplayer game (Higher or Lower)
│       ├── build/                # Compiled game executable (build/game)
│       ├── docs/                 # ARCHITECTURE.md and GAME_DEVELOPMENT.md
│       ├── src/                  # Game rules (game.cpp, player.cpp) and TCP lib/
│       ├── Makefile              # Compiles binary into games/template/build/game
│       └── README.md             # Template overview and quickstart guide
├── src/
│   ├── client/                   # Terminal User Interface (TUI) Client
│   │   ├── docs/                 # ARCHITECTURE.md, SCREEN_PROTOCOL.md, USER_GUIDE.md
│   │   ├── src/                  # Modular source code: core/, network/, and ui/
│   │   ├── help.txt              # External hot-reloadable help menu
│   │   ├── Makefile              # Standalone client build file
│   │   └── README.md             # Client module overview
│   ├── server/                   # Central Lobby Server
│   │   ├── docs/                 # CLIENT_PROTOCOL.md, GAME_INTEGRATION.md, ARCHITECTURE.md
│   │   ├── src/                  # Modular source code: core/, network/, and room/
│   │   ├── game.config           # Game catalog configuration (LOCAL and REMOTE)
│   │   ├── Makefile              # Standalone server build file
│   │   └── README.md             # Server module overview
│   └── Makefile                  # Unified Makefile for src/ modules
├── Makefile                      # Root Makefile (builds and runs the entire ecosystem)
└── README.md                     # This document
```

### 5.1. Reading Guide for Creating a New Game

The **SelectJogos** platform is designed so developers can build and plug in new multiplayer games (in C++ or any language supporting TCP sockets) without modifying a single line of the Lobby Server or TUI Client.

If you want to create a new game for the platform, read the following documents in order:

1. **[Lobby $\leftrightarrow$ Game Integration Specification (`src/server/docs/GAME_INTEGRATION.md`)](./src/server/docs/GAME_INTEGRATION.md)** *(Required Reading)*
   * Explains how to register your game in `src/server/game.config` using either **`LOCAL`** mode (spawned on demand by the Lobby via `fork`/`exec`) or **`REMOTE`** mode (connecting to an external game server).
   * Specifies the inbound protocol `<Command> <MsgID> <EffectiveClientID> [Parameters...]` (including automatic lifecycle commands `ConnectClient`, `DisconnectClient`, `ReconnectClient`, player moves, and host `ClientID = 0` actions) and outbound replies (`Response` and `LogChannel`).
2. **[Game Screen Sub-Protocol Specification (`src/client/docs/SCREEN_PROTOCOL.md`)](./src/client/docs/SCREEN_PROTOCOL.md)** *(Recommended for Visual Games)*
   * Details how to embed optional `@SCREEN`, `@LINE`, `@CLEAR`, and `@ALERT` directives inside `LogChannel` messages to render persistent ASCII boards, scoreboards, and turn alerts inside the Client TUI's **Game Canvas** (supporting both shared `All` views and targeted `<IdList>` views).
3. **[C++ Game Template Documentation (`games/template/README.md` & `games/template/docs/`)](./games/template/README.md)** *(Fastest C++ Starting Point)*
   * Shows how to copy `games/template/`—which already includes the reusable TCP networking layer (`src/lib/`) and an $O(1)$ command dispatcher—so you only need to implement your game rules inside the `Game` class.

### 5.2. Additional System Manuals

* **For Players / End Users:**
  * [`src/client/docs/USER_GUIDE.md`](./src/client/docs/USER_GUIDE.md) — TUI zone layout, case-insensitive command shorthands (`lg`, `cr`, `jr`, `sg`, `pa`), match walkthrough, and `help.txt` customization.
* **For Platform Maintainers:**
  * [`src/server/docs/CLIENT_PROTOCOL.md`](./src/server/docs/CLIENT_PROTOCOL.md) — Wire protocol specification between the Client and Lobby Server.
  * [`src/server/docs/ARCHITECTURE.md`](./src/server/docs/ARCHITECTURE.md) — Lobby Server internal concurrency, room lifecycle, and mutex lock hierarchy.
  * [`src/client/docs/ARCHITECTURE.md`](./src/client/docs/ARCHITECTURE.md) — Client Reactive MVC design, snapshot rendering, and `<termios.h>` engine.

---

## 6. Building and Running the System

The root `Makefile` orchestrates building the template game (`games/template/build/game`), the Lobby Server (`src/server/lobby`), and the TUI Client (`src/client/client`).

### 6.1. Compile Everything
From the repository root (`SelectJogos/`):

```bash
make
```

### 6.2. Single-Terminal Quick Run (Background Server + Foreground TUI Client)
To compile all modules, launch the Lobby Server in the background (redirecting server logs to `src/server/lobby.log`), and immediately open the interactive TUI Client:

```bash
make run
```
*(Typing `exit` in the client automatically terminates the background Lobby Server).*

### 6.3. Multi-Terminal Mode (Recommended for Live Presentation & Multiplayer Testing)
To observe real-time server logs and connect multiple concurrent players, open **3 terminals** at the repository root:

* **Terminal 1 (Lobby Server):**
  ```bash
  make run-server PORT=8080
  ```
* **Terminal 2 (Player 1 — Room Host):**
  ```bash
  make run-client HOST=127.0.0.1 PORT=8080
  ```
* **Terminal 3 (Player 2 — Challenger):**
  ```bash
  make run-client HOST=127.0.0.1 PORT=8080
  ```

### 6.4. Clean Build Artifacts
To remove all `objs/` and `build/` directories, binaries, and log files across the entire repository:

```bash
make clean
```