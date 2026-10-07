# Campo Minado Protocol & Development

This guide details the specific Lobby Server commands and TUI (Text User Interface) directives used to render and control Campo Minado.

---

## 1. Supported Client Commands

The Game Server registers the following commands, which are mapped to TUI aliases in the Lobby Client.

### 1.1. In-Game Aliases (`pa <move>`)
Any text sent by the client not prefixed with `!` is wrapped into `PlayerAction <text>`.
* **Reveal Cell:** `pa A1` (or just `A1` in the client).
* **Place Flag:** `pa f B3` (aliases: `flag`, `f`).
* **Remove Flag:** `pa u B3` (aliases: `unflag`, `u`).

### 1.2. Meta Commands
* **`!start`** -> `StartGame`
* **`!name <team_name>`** -> `NameTeam <team_name>` (Any player can execute this after winning).
* **`!rank <dificuldade>`** -> `Ranking <easy|medium|hard>`

---

## 2. Rendering the Game (TUI Directives)

Campo Minado uses the TUI Canvas protocol (`@SCREEN_TAG`) to dynamically update the players' screens without spamming standard output logs. 

### 2.1. Screen Broadcasting
Every time a player makes a move, the `Game` rebuilds the entire screen as a single string and sends it via `LogChannel All`:

```cpp
std::string board_render = "@SCREEN_TAG ";
board_render += GetRoomPlayersString(r->GetId());
board_render += "[ STATUS: JOGANDO ]\n\n";
board_render += r->GetBoard().Render();
BroadcastToRoom(room_id, board_render, server);
```

### 2.2. The `@SCREEN_TAG` Directive
When the Lobby Client receives a packet starting with `@SCREEN_TAG`:
1. It clears the dedicated Game Canvas area above the chat prompt.
2. It prints the rest of the string exactly as formatted.
3. It preserves ANSI color codes (used by `Board::Render` to colorize numbers and bombs).

This guarantees that all players see exactly the same grid layout at the same time, without their local terminals getting corrupted by scrolling chat logs.

---

## 3. The Game Over Mechanic

Instead of adding a penalty and continuing, stepping on a mine in Campo Minado immediately ends the current game. 

In `Game::HandlePlayerAction`:
```cpp
bool hit_bomb = r->GetBoard().Reveal(parsed_move.row, parsed_move.col);
if (hit_bomb) {
    auto now = std::chrono::steady_clock::now();
    int elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - r->GetStartTime()).count();
    r->SetLost(elapsed);
    r->GetBoard().RevealMines(parsed_move.row, parsed_move.col);
    // ...
}
```

The board state freezes in `LOST` mode, revealing all the bombs and showing an exploding animation on the fatal block. The team must type `!start` to reset the room and try again.
