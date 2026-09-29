# Game Screen Sub-Protocol Specification

The **Lobby Client** supports an optional, backwards-compatible visual sub-protocol embedded inside standard `LogChannel` messages. This allows games to render persistent ASCII boards, scoreboards, status panels, and custom alert banners in dedicated zones of the Terminal User Interface (TUI) without modifying the Lobby Server.

---

## 1. Design Philosophy & Backwards Compatibility

The Lobby Server routes `LogChannel <Target> "<Message>"` packets transparently from the Game Server to the appropriate clients. The Client's `ProtocolParser` inspects the `<Message>` payload upon arrival:

1. **Standard Text Mode (Default Fallback):** If `<Message>` does **not** begin with a registered `@` directive, the Client hides the **Game Canvas** zone (allocating 100% of the central viewport to the scrolling **Log Feed**) and appends the message with its channel tag (`[System]`, `[Room]`, `[Game]`, or `[Private]`).
2. **Graphical Canvas Mode (`@` Directives):** If `<Message>` starts with `@` and the first token matches a registered directive in `_screen_directives`, the Client intercepts the packet, updates the **Game Canvas** or **Alert Bar**, and redraws the screen immediately without cluttering the scrolling Log Feed.

> **Note:** Implementing this sub-protocol in a game is **strictly optional**. Games that only emit plain-text `LogChannel` messages work out of the box.

---

## 2. Visual Viewport Layout

When a game activates the canvas via `@SCREEN` or `@LINE`, the Client TUI automatically opens a bordered box at the top of the viewport while keeping the rolling log history and input bar intact below it:

```text
 LOBBY CLIENT | ID: 904028 | Room: Arena1 (747671)                 [Playing] 
┌────────────────────────────────────────────────────────────────────────────┐
│ [ROUND 2] Higher or Lower                                                  │
│ Player 904028: [ READY ]   |   Player 296846: [ THINKING... ]              │
│ Score: 1 - 0                                                               │
└────────────────────────────────────────────────────────────────────────────┘
[Room] Game HigherLower started in room Arena1
[Private] You chose number 500
[Game] Player 904028 won!
                                                                              
[!] ERROR: You have already played in this round                              
├────────────────────────────────────────────────────────────────────────────┤
> pa 250_                                                                     
```

---

## 3. Directive Reference

All directives are sent by the Game Server using the standard Lobby `LogChannel` syntax:

```text
LogChannel <Target> "<Directive> [Arguments...]"
```

| Directive | Syntax | Behavior |
| :--- | :--- | :--- |
| **`@SCREEN`** | `@SCREEN <Line_0> \| <Line_1> \| ...` | Replaces the entire Game Canvas with the provided lines separated by the pipe character (`\|`). Leading and trailing whitespace around each segment is trimmed automatically. |
| **`@LINE`** | `@LINE <Index> <Text>` | Updates a single 0-indexed row (`<Index>`) of the Game Canvas. If `<Index>` exceeds the current number of canvas lines, intermediate lines are padded with empty strings. |
| **`@CLEAR`** | `@CLEAR` | Clears all lines in the Game Canvas and collapses the top box, returning its vertical space to the Log Feed. |
| **`@ALERT`** | `@ALERT <Text>` | Displays `<Text>` in bold red inside the persistent **Alert Bar** directly above the input prompt. Cleared automatically on the player's next successful command. |

---

## 4. Detailed Directive Behavior

### 4.1. `@SCREEN` — Full Canvas Replacement
Use `@SCREEN` when rendering or refreshing an entire board or status panel at once.
* **Delimiter:** The pipe character (`|`) splits the payload into individual horizontal rows.
* **Empty Lines:** An empty segment between pipes (e.g., `@SCREEN Title | | Footer`) renders a blank line inside the canvas box.
* **Wire Example:**
  ```text
  LogChannel All "@SCREEN === TIC-TAC-TOE === | . | [X][ ][O] | [ ][X][ ] | [ ][ ][ ] | Turn: Player 904028"
  ```

### 4.2. `@LINE` — Partial Row Update
Use `@LINE` to update a specific row (such as a timer, status line, or a single row of a grid) without resending the entire board.
* **`<Index>`:** Zero-based row number (`0` is the first line inside the canvas box, up to `29`).
* **Wire Example:**
  ```text
  LogChannel All "@LINE 5 Turn: Player 296846"
  ```

### 4.3. `@CLEAR` — Collapse Canvas
Use `@CLEAR` when a match ends or returns to a state that no longer requires a persistent visual board.
* *Note:* The Client also clears the canvas automatically if the match stops (`StopGame`), the game server closes the connection, or the player leaves the room (`LeaveRoom`).
* **Wire Example:**
  ```text
  LogChannel All "@CLEAR"
  ```

### 4.4. `@ALERT` — Persistent Warning Banner
Use `@ALERT` when the game wants to highlight an urgent asynchronous notification (e.g., `"Your turn! Make a move within 15 seconds"`) without waiting for a command `Response Fail`.
* **Wire Example:**
  ```text
  LogChannel 904028 "@ALERT Your turn! Use 'pa <number>' to play."
  ```

---

## 5. Broadcast vs. Private Player Views

Because directives are transported inside `LogChannel <Target>`, games can choose whether all players see the exact same screen or individualized private screens:

| Target (`<Channel>`) | Use Case | Example |
| :--- | :--- | :--- |
| **`All`** | **Shared Public Board:** Chess, Checkers, Tic-Tac-Toe, or public scoreboards where every participant in the room sees the same state. | `LogChannel All "@SCREEN Score: 2 - 1 \| Round 4"` |
| **`<ClientID>`** *(or `Id1-Id2`)* | **Asymmetric / Private View:** Card games, Battleship, or Higher/Lower where each player must see their own secret hand or status on the canvas. | `LogChannel 904028 "@SCREEN Your Secret Number: [ 500 ] \| Opponent: [ HIDDEN ]"` |

---

## 6. Constraints & Best Practices

1. **Maximum Canvas Height (`MAX_CANVAS_LINES = 30`):**
   `ClientState` caps the canvas at **30 lines** to prevent memory abuse. For standard `24x80` terminal windows, keep the canvas between **4 and 12 lines** so at least 3 lines remain visible in the scrolling Log Feed below.
2. **Horizontal Width (`<= 74` characters recommended):**
   The canvas box adds 4 columns of horizontal framing (`"│ "` on the left and `" │"` on the right). Keeping each segment at or below **74 ASCII characters** ensures lines are never truncated on standard 80-column terminals.
3. **Avoid Pipe (`|`) Characters Inside `@SCREEN` Text:**
   Because `@SCREEN` splits lines on `|`, do not use `|` as a decorative border inside `@SCREEN` payloads. If a line itself must contain the `|` character, send that row via `@LINE <Index> <Text>` (which does not split on `|`).
4. **Single-Line Framing:**
   Every `LogChannel` packet sent over TCP must be a single line terminated by `\n`. Never embed raw newline characters (`\n`) inside the quoted `<Message>`; always use `|` in `@SCREEN` or separate `@LINE` commands.

---

## 7. C++ Game Integration Example

Below is an example of how a C++ game using the project's template (`Game::HandlePlayerAction`) can combine a private `@SCREEN` update with a public log message:

```cpp
// Update Player 1's private canvas with their chosen number
server.SendMessage(
    socket_fd,
    "LogChannel " + std::to_string(client_id) +
    " \"@SCREEN === HIGHER OR LOWER === | Your Move: [ " + std::to_string(number) +
    " ] | Status: Waiting for opponent...\"\n"
);

// When the round finishes, update everyone's canvas with the final result and log the winner
if (active_players.size() >= REQUIRED_PLAYERS) {
    std::string p1_str = "P1 (" + std::to_string(active_players[0]->getId()) + "): " +
                         std::to_string(active_players[0]->getNumber());
    std::string p2_str = "P2 (" + std::to_string(active_players[1]->getId()) + "): " +
                         std::to_string(active_players[1]->getNumber());

    server.SendMessage(
        socket_fd,
        "LogChannel All \"@SCREEN === ROUND FINISHED === | " + p1_str + " vs " + p2_str +
        " | Result: " + result + "\"\n"
    );
    server.SendMessage(socket_fd, "LogChannel All \"" + result + "\"\n");
}
```