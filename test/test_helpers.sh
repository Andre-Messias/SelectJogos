#!/bin/bash

# Resolve project directories relative to the test/ folder
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$TEST_DIR/.." && pwd)"
SERVER_DIR="$PROJECT_ROOT/src/server"
WORK_DIR="$TEST_DIR/tmp"

LOBBY_PORT=8080
REMOTE_GAME_PORT=8081

# Automatically detect configured game names and ports from game.config (with fallbacks)
LOCAL_GAME_NAME=$(awk '!/^[[:space:]]*#/ && $2 == "LOCAL" {print $1; exit}' "$SERVER_DIR/game.config" 2>/dev/null)
REMOTE_GAME_NAME=$(awk '!/^[[:space:]]*#/ && $2 == "REMOTE" {print $1; exit}' "$SERVER_DIR/game.config" 2>/dev/null)
DETECTED_REMOTE_PORT=$(awk '!/^[[:space:]]*#/ && $2 == "REMOTE" {print $4; exit}' "$SERVER_DIR/game.config" 2>/dev/null)

LOCAL_GAME_NAME="${LOCAL_GAME_NAME:-HigherLower}"
REMOTE_GAME_NAME="${REMOTE_GAME_NAME:-HigherLowerExt}"
REMOTE_GAME_PORT="${DETECTED_REMOTE_PORT:-8081}"

# Resolve the local game binary path from game.config or default build directories
DETECTED_REL_BIN=$(awk '!/^[[:space:]]*#/ && $2 == "LOCAL" {print $3; exit}' "$SERVER_DIR/game.config" 2>/dev/null)
if [ -n "$DETECTED_REL_BIN" ] && [ -f "$SERVER_DIR/$DETECTED_REL_BIN" ]; then
    GAME_BIN="$SERVER_DIR/$DETECTED_REL_BIN"
elif [ -f "$PROJECT_ROOT/games/template/build/game" ]; then
    GAME_BIN="$PROJECT_ROOT/games/template/build/game"
else
    GAME_BIN="$PROJECT_ROOT/games/build/template/game"
fi

PASS_COUNT=0
FAIL_COUNT=0

# Clean up background processes, file descriptors, and temporary FIFO/log files on exit
cleanup_env() {
    exec 3>&- 4>&- 5>&- 6>&- 2>/dev/null
    kill $LOBBY_PID $REMOTE_GAME_PID $NC1_PID $NC2_PID $NC3_PID $NC4_PID 2>/dev/null
    wait $LOBBY_PID $REMOTE_GAME_PID $NC1_PID $NC2_PID $NC3_PID $NC4_PID 2>/dev/null
    rm -rf "$WORK_DIR"
}

# Start the REMOTE game server, the Lobby server, and 4 persistent Netcat client pipes
setup_env() {
    mkdir -p "$WORK_DIR"
    rm -f "$WORK_DIR"/fifo_p* "$WORK_DIR"/out_p*.log

    echo "=== Starting External Game Server (REMOTE) on port $REMOTE_GAME_PORT ==="
    "$GAME_BIN" "$REMOTE_GAME_PORT" > "$WORK_DIR/remote_game.log" 2>&1 &
    REMOTE_GAME_PID=$!

    echo "=== Starting Lobby Server on port $LOBBY_PORT ==="
    (cd "$SERVER_DIR" && ./lobby "$LOBBY_PORT" game.config > "$WORK_DIR/lobby.log" 2>&1) &
    LOBBY_PID=$!
    sleep 0.3

    # Create named pipes (FIFOs) to control 4 players via Netcat
    mkfifo "$WORK_DIR/fifo_p1" "$WORK_DIR/fifo_p2" "$WORK_DIR/fifo_p3" "$WORK_DIR/fifo_p4"

    nc 127.0.0.1 "$LOBBY_PORT" < "$WORK_DIR/fifo_p1" > "$WORK_DIR/out_p1.log" & NC1_PID=$!
    nc 127.0.0.1 "$LOBBY_PORT" < "$WORK_DIR/fifo_p2" > "$WORK_DIR/out_p2.log" & NC2_PID=$!
    nc 127.0.0.1 "$LOBBY_PORT" < "$WORK_DIR/fifo_p3" > "$WORK_DIR/out_p3.log" & NC3_PID=$!
    nc 127.0.0.1 "$LOBBY_PORT" < "$WORK_DIR/fifo_p4" > "$WORK_DIR/out_p4.log" & NC4_PID=$!

    # Keep FIFO write descriptors open (FDs 3, 4, 5, and 6)
    exec 3>"$WORK_DIR/fifo_p1" 4>"$WORK_DIR/fifo_p2" 5>"$WORK_DIR/fifo_p3" 6>"$WORK_DIR/fifo_p4"

    # Wait until all 4 clients receive their initial ClientID welcome message
    wait_for_log "$WORK_DIR/out_p1.log" "Connected with ClientID"
    wait_for_log "$WORK_DIR/out_p2.log" "Connected with ClientID"
    wait_for_log "$WORK_DIR/out_p3.log" "Connected with ClientID"
    wait_for_log "$WORK_DIR/out_p4.log" "Connected with ClientID"
}

# Poll a log file for up to 3 seconds (60 * 50ms) until the target pattern appears
wait_for_log() {
    local log_file="$1"
    local pattern="$2"
    for _ in $(seq 1 60); do
        if grep -qF "$pattern" "$log_file" 2>/dev/null; then
            return 0
        fi
        sleep 0.05
    done
    return 1
}

# Assert that a pattern appears in the given log file and record PASS/FAIL
assert_log() {
    local log_file="$1"
    local pattern="$2"
    local description="$3"

    if wait_for_log "$log_file" "$pattern"; then
        echo -e "\033[0;32m[PASS]\033[0m $description"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo -e "\033[0;31m[FAIL]\033[0m $description"
        echo "       Expected pattern: '$pattern' in $(basename "$log_file")"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
}

# Extract the ClientID assigned by the Lobby from a client's log file
get_client_id() {
    local log_file="$1"
    grep "Connected with ClientID" "$log_file" | awk '{print $6}' | tr -d '"\r'
}

# Extract the RoomID returned by a CreateRoom <msg_id> response
get_room_id() {
    local log_file="$1"
    local msg_id="$2"
    wait_for_log "$log_file" "Response $msg_id Success"
    grep "Response $msg_id Success" "$log_file" | awk '{print $4}' | tr -d '\r'
}

# Print final test statistics and exit with non-zero code if any assertion failed
print_summary() {
    echo ""
    echo "========================================================="
    echo "Test Summary: $PASS_COUNT passed | $FAIL_COUNT failed"
    echo "========================================================="
    if [ "$FAIL_COUNT" -gt 0 ]; then
        exit 1
    fi
}