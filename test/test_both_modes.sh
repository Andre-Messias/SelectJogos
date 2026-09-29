#!/bin/bash

source "$(dirname "${BASH_SOURCE[0]}")/test_helpers.sh"
trap cleanup_env EXIT

setup_env

P1_ID=$(get_client_id "$WORK_DIR/out_p1.log")
P2_ID=$(get_client_id "$WORK_DIR/out_p2.log")
P3_ID=$(get_client_id "$WORK_DIR/out_p3.log")
P4_ID=$(get_client_id "$WORK_DIR/out_p4.log")

echo "-> Connected Clients: P1=$P1_ID, P2=$P2_ID, P3=$P3_ID, P4=$P4_ID"

echo "=== 1. Player 1 lists games and creates LocalRoom (LOCAL Mode) ==="
echo "ListGames m1" >&3
assert_log "$WORK_DIR/out_p1.log" "Response m1 Success" "ListGames returned success"

echo "CreateRoom m2 LocalRoom $LOCAL_GAME_NAME 123" >&3
LOCAL_ROOM_ID=$(get_room_id "$WORK_DIR/out_p1.log" "m2")
echo "-> LocalRoom created with RoomID: $LOCAL_ROOM_ID"

echo "=== 2. Player 2 joins LocalRoom and Player 1 starts the LOCAL game ==="
echo "JoinRoom m3 $LOCAL_ROOM_ID 123" >&4
assert_log "$WORK_DIR/out_p2.log" "Response m3 Success" "Player 2 joined private LocalRoom"
assert_log "$WORK_DIR/out_p1.log" "ClientID $P2_ID joined the room" "Player 1 received join notification for Player 2"

echo "StartGame m4" >&3
assert_log "$WORK_DIR/out_p1.log" "Response m4 Success" "Player 1 started LOCAL game"
assert_log "$WORK_DIR/out_p2.log" "Game $LOCAL_GAME_NAME started in room LocalRoom" "LocalRoom broadcast match start"

echo "=== 3. Player 3 creates RemoteRoom (REMOTE Mode) ==="
echo "CreateRoom m5 RemoteRoom $REMOTE_GAME_NAME" >&5
REMOTE_ROOM_ID=$(get_room_id "$WORK_DIR/out_p3.log" "m5")
echo "-> RemoteRoom created with RoomID: $REMOTE_ROOM_ID"

echo "=== 4. Player 4 joins RemoteRoom and Player 3 starts the REMOTE game ==="
echo "JoinRoom m6 $REMOTE_ROOM_ID" >&6
assert_log "$WORK_DIR/out_p4.log" "Response m6 Success" "Player 4 joined public RemoteRoom"

echo "StartGame m7" >&5
assert_log "$WORK_DIR/out_p3.log" "Response m7 Success" "Player 3 started REMOTE game"
assert_log "$WORK_DIR/out_p4.log" "Game $REMOTE_GAME_NAME started in room RemoteRoom" "RemoteRoom broadcast match start"

echo "=== 5. Executing moves simultaneously in both rooms ==="
# LOCAL Room: Player 1 plays 500, Player 2 plays 200 (Player 1 must win)
echo "PlayerAction m8 500" >&3
echo "PlayerAction m9 200" >&4

# REMOTE Room: Player 3 plays 100, Player 4 plays 900 (Player 4 must win)
echo "PlayerAction m10 100" >&5
echo "PlayerAction m11 900" >&6

assert_log "$WORK_DIR/out_p1.log" "You chose number 500" "Private LogChannel delivered to Player 1"
assert_log "$WORK_DIR/out_p1.log" "Player $P1_ID won!" "Player 1 won in LOCAL room (P1 view)"
assert_log "$WORK_DIR/out_p2.log" "Player $P1_ID won!" "Player 1 won in LOCAL room (P2 view)"

assert_log "$WORK_DIR/out_p3.log" "Player $P4_ID won!" "Player 4 won in REMOTE room (P3 view)"
assert_log "$WORK_DIR/out_p4.log" "Player $P4_ID won!" "Player 4 won in REMOTE room (P4 view)"

echo ""
echo "==================== CLIENT OUTPUT LOGS ===================="
echo "--- PLAYER 1 (LOCAL Room) ---"
cat "$WORK_DIR/out_p1.log"
echo "--- PLAYER 2 (LOCAL Room) ---"
cat "$WORK_DIR/out_p2.log"
echo "--- PLAYER 3 (REMOTE Room) ---"
cat "$WORK_DIR/out_p3.log"
echo "--- PLAYER 4 (REMOTE Room) ---"
cat "$WORK_DIR/out_p4.log"

print_summary