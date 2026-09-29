#!/bin/bash

source "$(dirname "${BASH_SOURCE[0]}")/test_helpers.sh"
trap cleanup_env EXIT

setup_env

P1_ID=$(get_client_id "$WORK_DIR/out_p1.log")
P2_ID=$(get_client_id "$WORK_DIR/out_p2.log")
P3_ID=$(get_client_id "$WORK_DIR/out_p3.log")

echo "=== 1. Creating room, testing password validation, and starting match ==="
echo "CreateRoom c1 TestArena $LOCAL_GAME_NAME 999" >&3
ROOM_ID=$(get_room_id "$WORK_DIR/out_p1.log" "c1")

echo "JoinRoom j_err $ROOM_ID wrong_pass" >&4
assert_log "$WORK_DIR/out_p2.log" "Response j_err Fail \"Incorrect password\"" "Reject JoinRoom with wrong password"

echo "JoinRoom j1 $ROOM_ID 999" >&4
assert_log "$WORK_DIR/out_p2.log" "Response j1 Success" "Player 2 joins with valid password"

echo "StartGame s1" >&3
assert_log "$WORK_DIR/out_p1.log" "Response s1 Success" "Match started by room creator"

echo "=== 2. Testing maximum 2-player limit enforcement ==="
echo "JoinRoom j2 $ROOM_ID 999" >&5
assert_log "$WORK_DIR/out_p3.log" "Response j2 Success" "Player 3 joins the Lobby room"
assert_log "$WORK_DIR/out_p3.log" "Connection refused by game: match already has 2 players." "Game refuses 3rd player via targeted LogChannel"

echo "PlayerAction a_p3 999" >&5
assert_log "$WORK_DIR/out_p3.log" "Response a_p3 Fail \"Player Not Connected\"" "Game blocks PlayerAction from rejected 3rd player"

echo "KickPlayer k1 $P3_ID" >&3
assert_log "$WORK_DIR/out_p1.log" "Response k1 Success" "Room creator kicks Player 3"
assert_log "$WORK_DIR/out_p3.log" "You have been kicked from room TestArena" "Player 3 receives kick notification"

echo "=== 3. Testing duplicate move prevention and ServerAction (ResetRound) ==="
echo "PlayerAction a1 400" >&3
assert_log "$WORK_DIR/out_p1.log" "Response a1 Success" "Player 1 first move accepted"

echo "PlayerAction a2 800" >&3
assert_log "$WORK_DIR/out_p1.log" "Response a2 Fail \"You have already played in this round\"" "Game blocks duplicate move in the same round"

echo "ServerAction sa_fail ResetRound" >&4
assert_log "$WORK_DIR/out_p2.log" "Response sa_fail Fail" "Lobby blocks ServerAction from non-creator (Player 2)"

echo "ServerAction sa_ok ResetRound" >&3
assert_log "$WORK_DIR/out_p1.log" "Response sa_ok Success" "Creator resets round via ServerAction (Client 0)"
assert_log "$WORK_DIR/out_p2.log" "Round reset by the room creator!" "Room receives ResetRound broadcast"

echo "=== 4. Testing identical MsgID ('same_id') collision isolation ==="
echo "PlayerAction same_id 300" >&3
echo "PlayerAction same_id 300" >&4
assert_log "$WORK_DIR/out_p1.log" "Response same_id Success" "Player 1 receives Response same_id without collision"
assert_log "$WORK_DIR/out_p2.log" "Response same_id Success" "Player 2 receives Response same_id without collision"
assert_log "$WORK_DIR/out_p1.log" "It is a tie!" "Tie result broadcast and round automatically reset"

echo "=== 5. Testing DisconnectClient, ReconnectClient, and Host Migration ==="
echo "LeaveRoom l1" >&4
assert_log "$WORK_DIR/out_p2.log" "Response l1 Success" "Player 2 leaves room during active match"
assert_log "$WORK_DIR/out_p1.log" "Player $P2_ID left the match." "Game processes DisconnectClient for Player 2"

echo "JoinRoom j_recon $ROOM_ID 999" >&4
assert_log "$WORK_DIR/out_p2.log" "Response j_recon Success" "Player 2 rejoins the room"
assert_log "$WORK_DIR/out_p1.log" "Player $P2_ID returned to the match." "Game processes ReconnectClient for Player 2"

echo "LeaveRoom l2" >&3
assert_log "$WORK_DIR/out_p1.log" "Response l2 Success" "Original creator (Player 1) leaves the room"
assert_log "$WORK_DIR/out_p2.log" "ClientID $P2_ID is now the room creator" "Ownership migrates to Player 2"

echo "StopGame stop1" >&4
assert_log "$WORK_DIR/out_p2.log" "Response stop1 Success" "New creator (Player 2) stops the game via StopGame"

print_summary