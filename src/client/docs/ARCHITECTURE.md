# Client Architecture & Maintenance Guide

The **Lobby Client** is a multi-threaded C++17 Terminal User Interface (TUI) application that provides a structured, flicker-free graphical console experience for interacting with the Lobby Server and its hosted games. Built strictly on standard POSIX headers and ANSI escape sequences.

---

## 1. Architectural Overview (Reactive Terminal MVC)

The codebase follows a **Reactive Model-View-Controller (MVC)** architecture decoupled across three internal domains (`core`, `network`, and `ui`):

### Core Design Principles
* **Immutable Snapshot Rendering:** The UI thread never holds the state mutex while calculating layout dimensions or writing to `stdout`. Instead, `ClientState::GetSnapshot()` copies the current state under lock in $O(N)$ memory time and returns an independent `ScreenSnapshot` value object.
* **Single-Write Frame Assembly:** Every screen redraw is assembled completely in memory (`std::ostringstream`) and flushed to the terminal in a single stream operation, preventing cursor jumping or visual tearing when network messages arrive mid-typing.
* **Zero-Configuration Protocol Abstraction:** Users never manually type `<MsgID>` tokens. `ProtocolParser` automatically generates bounded message IDs, correlates asynchronous `Response` packets back to the originating command, and updates the header state machine (`[Lobby]`, `[Waiting]`, `[Playing]`).

---

## 2. Directory and Module Breakdown
### Module Responsibilities

| Module | Layer | Responsibility |
| :--- | :--- | :--- |
| **`ClientState`** (`src/core/`) | Model | Thread-safe repository for `ClientID`, `RoomID`, `RoomName`, `GameName`, `ClientScope`, the optional `_canvas_lines` buffer, rolling `_log_lines` history, `_alert_message`, `_input_buffer`, and `_sent_commands` map. |
| **`NetworkClient`** (`src/network/`) | Transport | Manages the lifecycle of the TCP socket (`Connect`, `Send`, `Disconnect`), shields against `SIGPIPE` (`MSG_NOSIGNAL`), and runs a dedicated background thread (`ListenLoop`) that frames incoming newline-delimited (`\n`) packets via `ReadToken`. |
| **`ProtocolParser`** (`src/network/`) | Controller | Translates user input (resolving case-insensitive shorthand aliases like `pa` $\rightarrow$ `PlayerAction` and reading `help.txt` on `Help`), injects bounded `MsgID` tokens, and parses inbound `Response` and `LogChannel` frames. |
| **`TerminalUI`** (`src/ui/`) | View | Manages terminal raw mode via RAII, switches to the Alternate Screen Buffer, reads keystrokes byte-by-byte from `STDIN_FILENO`, queries terminal dimensions via `ioctl(TIOCGWINSZ)`, and renders the 5-zone TUI layout. |
| **`main.cpp`** (`src/`) | Wiring | Validates CLI arguments (`<LOBBY_IP> <LOBBY_PORT> [HELP_FILE]`), installs `SIG_IGN` for `SIGPIPE`, connects lambdas between `NetworkClient`, `ProtocolParser`, and `TerminalUI`, and blocks on `ui.Run()`. |

---

## 3. Concurrency Model & Lock Hierarchy

The client runs with exactly **two threads**:

1. **Main / UI Thread:** Executes `TerminalUI::Run()`, blocking on `read(STDIN_FILENO, &c, 1)`. Mutates `_input_buffer` on keystrokes, invokes `ProtocolParser::HandleLocalInput()` on `Enter`, and calls `Render()`.
2. **Network Reader Thread:** Spawned by `NetworkClient::Connect()`, executing `NetworkClient::ListenLoop()`. Blocks on `read(fd, ...)` until a full `\n`-terminated line arrives, invokes `ProtocolParser::HandleServerMessage()`, and calls `TerminalUI::RefreshScreen()`.

### Mutex Synchronization Table

| Mutex | Owner Class | Protected Resources | Scope Duration |
| :--- | :--- | :--- | :--- |
| **`_state_mutex`** | `ClientState` | All session metadata, `_canvas_lines`, `_log_lines`, `_alert_message`, `_input_buffer`, and `_sent_commands`. | Short-lived; acquired only during individual getter/setter calls or snapshot cloning. |
| **`_render_mutex`** | `TerminalUI` | Terminal output stream (`std::cout`) and `Render()` execution. | Held across a single frame assembly and flush in `TerminalUI::Render()`. |
| **`_send_mutex`** | `NetworkClient` | Outbound TCP socket writes in `NetworkClient::Send()`. | Held until the entire outbound command string is transmitted. |

### Lock-Ordering Invariants (Deadlock Prevention)

To guarantee deadlock freedom across the UI and Network threads, the codebase enforces strict non-nested locking:
1. **`_state_mutex` is a leaf lock:** Methods inside `ClientState` never invoke callbacks or call into `NetworkClient` or `TerminalUI`.
2. **`_render_mutex` $\rightarrow$ `_state_mutex` (briefly):** When `TerminalUI::Render()` is called, it acquires `_render_mutex` first, immediately calls `_state.GetSnapshot()` (which acquires and releases `_state_mutex` to return a copy), and performs all string formatting and I/O after `_state_mutex` has been released.
3. **`_send_mutex` is independent:** `ProtocolParser::HandleLocalInput()` registers the sent command in `ClientState` (acquiring and releasing `_state_mutex`) **before** calling `_network.Send(payload)` (which acquires `_send_mutex`).

---

## 4. TUI Rendering Engine (`<termios.h>` & ANSI)

### 4.1. Terminal Raw Mode & Alternate Screen Buffer (RAII)
When `TerminalUI::EnableRawMode()` is called:
1. Original terminal attributes are saved into `_orig_termios` via `tcgetattr(STDIN_FILENO, &_orig_termios)`.
2. Local flags `ECHO` (automatic character echoing), `ICANON` (line-buffered canonical input), and `IEXTEN` are disabled via `tcsetattr(..., TCSAFLUSH, &raw)`.
3. The terminal switches to the **Alternate Screen Buffer** (`\033[?1049h\033[H\033[2J`), preserving the user's original shell scrollback history.
4. Upon exit (or destruction in `~TerminalUI()`), `DisableRawMode()` restores `_orig_termios` and emits `\033[?1049l`, returning the user's terminal to its exact pre-execution state.

### 4.2. Dynamic Viewport Allocation
On every `Render()` call, `ioctl(STDOUT_FILENO, TIOCGWINSZ, &w)` retrieves the current terminal height (`term_rows`) and width (`term_cols`), falling back to `24x80` if unavailable. Vertical rows are dynamically partitioned across 5 zones:

```text
Row 1                      : [1. Header Bar (Inverted Video \033[7m ... \033[0m)]
Rows 2 .. C+3 (Optional)   : [2. Game Canvas Box (┌───┐ ... └───┘, if _canvas_lines != empty)]
Remaining Middle Rows      : [3. Rolling Log Feed (Auto-scrolls to latest log_box_height lines)]
Row N-2 (Optional)         : [4. Alert / Error Bar (Bold Red \033[1;31m, if _alert_message != empty)]
Row N-1                    : [5. Input Separator (├───┤)]
Row N                      : [6. Input Prompt (> _input_buffer) + Explicit Cursor Placement]
```

### 4.3. Multi-Byte UTF-8 Safety
* **Box Drawing Borders:** Because UTF-8 box-drawing characters (`─`, `┌`, `┐`, `└`, `┘`, `├`, `┤`, `│`) occupy 3 bytes each, horizontal borders are constructed via `RepeatUTF8(count, "─")` rather than single-byte `char` constructors.
* **UTF-8 Backspace Handling:** `ClientState::BackspaceInput()` pops trailing UTF-8 continuation bytes (`(byte & 0xC0) == 0x80`) until it removes the leading byte of the code point, preventing half-erased multi-byte characters in the input buffer.

---

## 5. State Correlation & Protocol Invariants

### 5.1. Asynchronous Command Tracking (`_sent_commands`)
Because the Lobby protocol does not echo the command name inside `Response <MsgID> Success [Data]`, `ProtocolParser::HandleLocalInput()` records a `SentCommand{command, arg1, arg2}` in `ClientState` before transmitting:
* **`CreateRoom`:** Stores `arg1 = RoomName` and `arg2 = GameName`. When `Response <MsgID> Success <RoomID>` arrives, `HandleServerMessage()` parses `<RoomID>` and transitions the client into `ClientScope::ROOM_WAITING` with full room metadata in the header.
* **`JoinRoom`:** Stores `arg1 = RoomID`. On `Success`, transitions to `ClientScope::ROOM_WAITING`.
* **`LeaveRoom`:** On `Success`, clears room metadata, hides any active game canvas, and returns to `ClientScope::IN_LOBBY`.
* **`StartGame` / `StopGame`:** Transitions between `ROOM_PLAYING` and `ROOM_WAITING`.

### 5.2. Bounded `MsgID` Generation (Overflow Prevention)
`ProtocolParser::GenerateMsgId()` increments `_msg_counter` from `1` up to `MAX_MSG_COUNTER` (`999999`), wrapping back to `1` thereafter:
* Prevents unbounded string growth in `<MsgID>` tokens during long sessions.
* Avoids signed integer overflow UB and keeps IDs human-readable (`m1` .. `m999999`).
* Because `PopSentCommand()` removes entries immediately upon receiving a `Response` (and `RegisterSentCommand()` overwrites stale keys via `operator[]`), wrap-around never corrupts state tracking.

### 5.3. Memory Bounds
To prevent unbounded RAM growth during long sessions:
* `ClientState::MAX_LOG_HISTORY` caps `_log_lines` at **200 entries** (evicting the oldest via `std::deque::pop_front()`).
* `ClientState::MAX_CANVAS_LINES` caps `_canvas_lines` at **30 lines**.
* `NetworkClient::MAX_BUFFER_SIZE` caps the raw socket read buffer at **8192 bytes**, dropping malformed streams that omit newline delimiters.

---

## 6. Maintenance & Extension Guide

### 6.1. Adding a New Command Alias or Shorthand
To register a new shorthand (e.g., ` surrender ` / `sr` $\rightarrow$ `Surrender`):
1. Open `src/client/src/network/protocol_parser.cpp`.
2. Inside `ProtocolParser::RegisterAliases()`, add the lowercase mappings:
   ```cpp
   _command_aliases["sr"] = "Surrender";
   _command_aliases["surrender"] = "Surrender";
   ```
3. Document the new command and shorthand in `src/client/help.txt` (no recompilation required for `help.txt` edits).

### 6.2. Adding a New `@` Screen Directive
Visual directives inside `LogChannel` messages are dispatched in $O(1)$ time via the `_screen_directives` registry (`std::unordered_map<std::string, DirectiveHandler>`). To add a new directive (for example, `@CLEAR_ALERT` to dismiss the error banner from the game):
1. Open `src/client/src/network/protocol_parser.cpp`.
2. Inside `ProtocolParser::RegisterScreenDirectives()`, register the new directive token and its handler lambda:
   ```cpp
   _screen_directives["@CLEAR_ALERT"] = [this](std::istringstream& /*iss*/) {
       _state.ClearAlert();
   };

### 6.3. Adding a Local Client-Only Command
If a command should be executed locally by the client UI without sending network packets (like `Help` or a local `ClearLogs` command):
1. Register its aliases in `ProtocolParser::RegisterAliases()` (e.g., `_command_aliases["cl"] = "ClearLogs";`).
2. Intercept the canonical name at the top of `ProtocolParser::HandleLocalInput()` before `GenerateMsgId()` is called, mutate `_state`, and `return`.