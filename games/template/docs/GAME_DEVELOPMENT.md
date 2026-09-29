# Game Development Guide 
This guide explains how to create a new multiplayer game for the **SelectJogos** platform using `games/template/` as your starting point.

---

## 1. Quickstart: Cloning the Template

1. Copy the `games/template/` directory to a new folder under `games/` (for example, `games/tictactoe/`):
   ```bash
   cp -r games/template games/tictactoe
   cd games/tictactoe
   make clean
   ```
2. Keep `src/lib/server.hpp`, `src/lib/server.cpp`, `src/main.cpp`, and `Makefile` unchanged—they already implement the full TCP transport protocol required by the Lobby Server.
3. Customize `src/config.hpp`, `src/player.hpp/.cpp`, and `src/game.hpp/.cpp` with your new game's state and rules.

---

## 2. Step-by-Step Customization

### Step 1: Define Player State (`src/player.hpp` & `src/player.cpp`)
Modify the `Player` class to store whatever per-player state your game requires (e.g., coordinates, cards in hand, health points, or team symbol):

```cpp
class Player {
    public:
        explicit Player(int id);
        int getId() const;
        // Add custom getters/setters for your game:
        int getScore() const;
        void addScore(int points);
    private:
        int _id;
        int _score = 0;
};
```

### Step 2: Register Custom Commands (`Game::RegisterCommands`)
Open `src/game.cpp`. Inside `Game::RegisterCommands()`, bind the commands your game accepts. Every game should handle at least `PlayerAction`, `ServerAction`, and `PlayerDisconnected`:

```cpp
void Game::RegisterCommands() {
    _commands["PlayerAction"] = [this](int fd, int client_id, const std::string& msg_id, std::istringstream& iss) {
        HandlePlayerAction(fd, client_id, msg_id, iss);
    };

    _commands["ServerAction"] = [this](int fd, int client_id, const std::string& msg_id, std::istringstream& iss) {
        HandleServerAction(fd, client_id, msg_id, iss);
    };

    _commands["PlayerDisconnected"] = [this](int fd, int client_id, const std::string& msg_id, std::istringstream& iss) {
        HandlePlayerDisconnected(fd, client_id, msg_id, iss);
    };
}
```

> **Tip:** You can either route all player moves through `PlayerAction <SubAction> [Args...]` (which allows players to use the `pa` shorthand in the TUI Client, e.g., `pa A1` or `pa fold`) or register top-level command names directly in `_commands`.

### Step 3: Always Acknowledge Commands with `Response`
Whenever your handler processes a command with a non-zero `<MsgID>`, it **must** send exactly one `Response` line back to the Lobby Server:

* **On Valid Move:**
  ```cpp
  _server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
  ```
* **On Invalid Move / Rule Violation:**
  ```cpp
  _server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Not your turn!\"\n");
  return;
  ```
*(The TUI Client automatically displays the failure reason inside the red Alert Bar).*

### Step 4: Broadcast Events & Render TUI Screens via `LogChannel`
Use `LogChannel` to send text updates or graphical canvas directives (`@SCREEN`, `@LINE`, `@CLEAR`, `@ALERT`) to players:

* **Public Broadcast (to everyone in the room):**
  ```cpp
  _server.SendMessage(socket_fd, "LogChannel All \"Player " + std::to_string(client_id) + " scored!\"\n");
  ```
* **Private Message (to a single player):**
  ```cpp
  _server.SendMessage(socket_fd, "LogChannel " + std::to_string(client_id) + " \"Your secret card is [Ace of Spades]\"\n");
  ```
* **Graphical ASCII Board Update (in the TUI Game Canvas):**
  ```cpp
  _server.SendMessage(
      socket_fd,
      "LogChannel All \"@SCREEN === SCOREBOARD === | P1: 10 pts | P2: 8 pts\"\n"
  );
  ```
  *(See [`src/client/docs/SCREEN_PROTOCOL.md`](../../../src/client/docs/SCREEN_PROTOCOL.md) for full details on `@SCREEN`, `@LINE`, `@CLEAR`, and `@ALERT`).*

---

## 3. Registering Your Game in the Lobby Server

Once your game compiles into `games/<your_game>/build/game`:

1. Open `src/server/game.config`.
2. Add an entry for your game in either `LOCAL` or `REMOTE` mode:
   ```text
   # Format for LOCAL mode (Lobby spawns the binary automatically on StartGame):
   TicTacToe LOCAL ../../games/tictactoe/build/game

   # Format for REMOTE mode (Lobby connects to a pre-running server):
   TicTacToeRemote REMOTE 127.0.0.1 9005
   ```
3. (Optional) Add your new game directory to the root `Makefile` under the `games:` target so `make` builds it automatically alongside the rest of the project.