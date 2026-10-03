# Lobby Client — User Guide & Command Reference

The **Lobby Client** provides an interactive Terminal User Interface (TUI) for connecting to the Lobby Server, browsing games, managing rooms, and playing multiplayer matches without manually formatting raw TCP protocol tokens.

---

## 1. Launching the Client

### Using `make` (Recommended)
From the repository root (`SelectJogos/`) or from the `src/` directory, you can either launch both the Lobby Server and the Client together in a single terminal, or start only the Client to connect to an already running server:

```bash
# Start Lobby Server in background + Launch TUI Client
make run PORT=8080

# Launch only the TUI Client (connecting to an existing server)
make run-client HOST=127.0.0.1 PORT=8080 HELP=help.txt
```

### Direct Binary Execution
From `src/client/`:

```bash
./client <LOBBY_IP> <LOBBY_PORT> [HELP_FILE]
```

**Example:**
```bash
./client 127.0.0.1 8080 help.txt
```

---

## 2. Anatomy of the TUI Screen

The terminal window is divided into **5 dynamic zones** that automatically adapt to your terminal dimensions:

```text
 LOBBY CLIENT | ID: 904028 | Room: Arena1 (747671)                 [Playing]   <-- 1. Header Bar
┌────────────────────────────────────────────────────────────────────────────┐
│ === HIGHER OR LOWER ===                                                    │ <-- 2. Game Canvas
│ Your Move: [ 500 ] | Opponent: [ WAITING ]                                 │     (Optional)
└────────────────────────────────────────────────────────────────────────────┘
[System] Connected with ClientID 904028                                        <-- 3. Rolling Log Feed
[System] Room 'Arena1' created (ID: 747671).
[Room] ClientID 296846 joined the room
[Room] Game HigherLower started in room Arena1
[Private] You chose number 500
                                                                              
[!] ERROR: You have already played in this round                               <-- 4. Alert / Error Bar
├────────────────────────────────────────────────────────────────────────────┤
> pa 250_                                                                      <-- 5. Input Prompt
```

### Zone Breakdown
1. **Header Bar (Top Row):**
   * **`ID`:** Your unique `ClientID` assigned by the Lobby Server upon connection.
   * **`Room`:** Displays `Lobby` when outside a room, or `Room: <Name> (<RoomID>)` when inside a room.
   * **Scope Indicator (Right-aligned):**
     * `[Lobby]`: Browsing games or rooms.
     * `[Waiting]`: Inside a room waiting for the host to start the match.
     * `[Playing]`: Active match in progress.
2. **Game Canvas (Optional Top Box):**
   * Automatically appears when a game sends `@SCREEN` or `@LINE` visual directives. Collapses completely when playing text-only games or when returning to `[Waiting]` / `[Lobby]`.
3. **Rolling Log Feed (Center Viewport):**
   * Displays the latest chronological events prefixed by source tags:
     * `[System]`: Connection lifecycle and local room transitions.
     * `[Room]`: Players joining/leaving, host migrations, and match start/stop notifications.
     * `[Game]`: Public match broadcasts sent to all players in the room (`LogChannel All`).
     * `[Private]`: Secret or player-specific messages sent only to your `ClientID`.
     * `[Response]`: Server data replies (such as `ListGames` and `ListRooms` catalogs).
     * `[Error]`: History record of failed commands.
4. **Alert / Error Bar (Above Input Separator):**
   * Highlights the most recent error (`Response Fail`) or urgent game warning (`@ALERT`) in bold red without disrupting the game board. Automatically disappears as soon as your next command succeeds.
5. **Input Prompt (Bottom Row):**
   * Captures keystrokes in real time (`> `). Incoming network messages never overwrite or corrupt the text you are currently typing.

---

## 3. Automatic Protocol Abstraction

When using the TUI Client, two low-level protocol details are handled automatically:

1. **No Manual `<MsgID>` Required:**
   In the raw Lobby protocol, every command requires a message identifier (e.g., `CreateRoom m1 Arena1 HigherLower`). In the TUI Client, you omit `<MsgID>` completely and type only:
   ```text
   > CreateRoom Arena1 HigherLower
   ```
   The client automatically injects and tracks a unique `<MsgID>` in the background.
2. **Case-Insensitive Commands & Shorthands:**
   You can type full command names or two-letter shorthands in uppercase, lowercase, or mixed case (e.g., `PlayerAction 500`, `playeraction 500`, `PA 500`, and `pa 500` are all identical).

---

## 4. Complete Command & Shorthand Reference

| Category | Canonical Command | Shorthands (Case-Insensitive) | Syntax & Parameters | Description |
| :--- | :--- | :--- | :--- | :--- |
| **Discovery** | `ListGames` | `lg`, `listgames` | `lg` | Lists all games registered on the Lobby Server (`[Local]` and `[Remote]`). |
| **Discovery** | `ListRooms` | `lr`, `listrooms` | `lr` | Lists all active rooms, their `RoomID`, game name, privacy (`[Public]`/`[Private]`), and status (`[Waiting]`/`[Playing]`). |
| **Identity** | `SetNick` | `nick`, `!nick`, `setnick` | `nick <Name>` | Sets your display name (1–16 chars: letters, digits, `_`, `-`, `.`; must be unique). Works anywhere; inside a running match the new name appears immediately. Without one you show up as `Player <ClientID>`. |
| **Room** | `CreateRoom` | `cr`, `createroom` | `cr <RoomName> <GameName> [Password]` | Creates a new room for `<GameName>` (with optional `[Password]`) and joins it as the room creator (Host). |
| **Room** | `JoinRoom` | `jr`, `joinroom` | `jr <RoomID> [Password]` | Joins an existing room by its numeric `<RoomID>`. |
| **Room** | `LeaveRoom` | `lv`, `leaveroom` | `lv` | Leaves the current room and returns to `[Lobby]`. |
| **Host Only** | `StartGame` | `sg`, `startgame` | `sg` | Starts the match in the current room (requires being the room creator). |
| **Host Only** | `StopGame` | `st`, `stopgame` | `st` | Forcibly stops the running match and returns the room to `[Waiting]`. |
| **Host Only** | `KickPlayer` | `kp`, `kickplayer` | `kp <TargetClientID>` | Expels `<TargetClientID>` from the room. |
| **Host Only** | `ServerAction` | `sa`, `serveraction` | `sa <GameCommand> [Args...]` | Sends an administrative command to the running game as `ClientID 0`. Nested shorthands are also resolved (e.g., `sa rr`). |
| **In-Game** | `PlayerAction` | `pa`, `playeraction` | `pa <ActionArgs...>` | Sends a player move/action to the active game during `[Playing]`. |
| **In-Game Admin** | `ResetRound` | `rr`, `resetround` | `sa rr` | Resets the current round in the template game when invoked via `ServerAction`. |
| **Local Client** | `Help` | `h`, `help`, `?` | `h` *(or `help`, `?`)* | Loads and displays `help.txt` in the Log Feed without sending network traffic. |
| **Local Client** | `exit` / `quit` | `exit`, `quit` *(exact lowercase)* | `exit` *(or `quit`)* | Restores the terminal screen, closes the TCP connection, and exits the client. |

> **Custom Game Commands:** If a specific game defines additional custom commands beyond `PlayerAction` (for example, `DrawCard` or `PlaceShip A5`), simply type `<Command> [Args...]` at the prompt. Any unrecognized alias is forwarded as-is with an auto-generated `<MsgID>`.

---

## 5. Quick Walkthrough: Playing a Match

Below is a complete two-player example using shorthand commands:

### Player 1 (Room Creator / Host)
```text
> nick Alex_W                         # (Optional) Pick a display name
> lg                                  # Check available games
> cr Arena1 MaiorMenor 123            # Create private room 'Arena1' with password '123'
                                      # (Header updates to show RoomID, e.g., 747671)
                                      # Wait for Player 2 to join...
> sg                                  # Start the match!
> pa 500                              # Play number 500
> sa rr                               # (Optional) Administratively reset a round as Host
> lv                                  # Leave the room when finished
> exit                                # Close the client
```

### Player 2 (Challenger)
```text
> nick Pintudo                        # (Optional) Pick a display name
> lr                                  # Find the RoomID of 'Arena1' (e.g., ID:747671)
> jr 747671 123                       # Join room 747671 using password '123'
                                      # Wait for Player 1 to start the game...
> pa 200                              # Play number 200
> lv                                  # Leave the room
> exit                                # Close the client
```

---

## 6. Customizing the External Help Menu (`help.txt`)

The `Help` command (`h`, `help`, or `?`) reads its output dynamically from `src/client/help.txt` every time it is invoked. You can customize or translate the help menu at any time—even while the client is running—without recompiling the C++ source code.

### Formatting Rules for `help.txt`
* **Comments (`#`):** Any line starting with `#` is treated as a comment and ignored by the parser.
* **Blank Lines:** Empty lines are automatically skipped.
* **Width Recommendation:** Keep lines under **76 characters** so they fit cleanly on standard 80-column terminals without ellipsis (`...`) truncation.

### Default `src/client/help.txt`
```text
# Client Help File - Edit this file anytime without recompiling the code.
--- AVAILABLE COMMANDS (Full Name | Shorthand) ---
[Discovery]  ListGames (lg)                     - List available games
[Discovery]  ListRooms (lr)                     - List active rooms
[Identity]   SetNick (nick) <Name>              - Set display name (max 16)
[Room]       CreateRoom (cr) <Name> <Game> [Pw] - Create and join a room
[Room]       JoinRoom (jr) <RoomID> [Pw]        - Join an existing room
[Room]       LeaveRoom (lv)                     - Leave current room
[Host Only]  StartGame (sg)                     - Start the match
[Host Only]  StopGame (st)                      - Stop the running match
[Host Only]  KickPlayer (kp) <ClientID>         - Kick a player from room
[Host Only]  ServerAction (sa) <Cmd> [Args...]  - Admin action (e.g., 'sa rr')
[In-Game]    PlayerAction (pa) <Args...>        - Send move to running game
[Mines]      pa A5 | pa fA5 | pa uA5            - Reveal / flag / unflag
[Mines]      !start | !name <T> | !rank <d>     - Start / save team / ranking
[Client]     Help (h, ?) | exit / quit          - Show help or close client
--------------------------------------------------
```