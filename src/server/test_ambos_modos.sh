#!/bin/bash

# Limpa processos e pipes antigos ao sair
cleanup() {
    kill $LOBBY_PID $REMOTE_GAME_PID $NC1_PID $NC2_PID $NC3_PID $NC4_PID 2>/dev/null
    rm -f fifo_p1 fifo_p2 fifo_p3 fifo_p4 out_p1.log out_p2.log out_p3.log out_p4.log
}
trap cleanup EXIT

echo "=== 1. Iniciando Servidor Game Externo (REMOTE) na porta 8081 ==="
../../games/build/template/game 8081 &
REMOTE_GAME_PID=$!
sleep 0.5

echo "=== 2. Iniciando Servidor Lobby na porta 8080 ==="
./lobby 8080 game.config &
LOBBY_PID=$!
sleep 0.5

# Cria pipes para controlar os 4 jogadores via Netcat
mkfifo fifo_p1 fifo_p2 fifo_p3 fifo_p4

nc 127.0.0.1 8080 < fifo_p1 > out_p1.log & NC1_PID=$!
nc 127.0.0.1 8080 < fifo_p2 > out_p2.log & NC2_PID=$!
nc 127.0.0.1 8080 < fifo_p3 > out_p3.log & NC3_PID=$!
nc 127.0.0.1 8080 < fifo_p4 > out_p4.log & NC4_PID=$!

# Mantém os pipes abertos para escrita
exec 3>fifo_p1 4>fifo_p2 5>fifo_p3 6>fifo_p4
sleep 0.5

echo "=== 3. Jogador 1 cria SalaLocal (Modo LOCAL) ==="
echo "ListGames m1" >&3
echo "CreateRoom m2 SalaLocal MaiorMenor 123" >&3
sleep 0.3

# Captura o RoomID aleatório gerado para a SalaLocal
LOCAL_ROOM_ID=$(grep "Response m2 Success" out_p1.log | awk '{print $4}')
echo "-> SalaLocal criada com RoomID: $LOCAL_ROOM_ID"

echo "=== 4. Jogador 2 entra na SalaLocal e Jogador 1 inicia o jogo LOCAL ==="
echo "JoinRoom m3 $LOCAL_ROOM_ID 123" >&4
sleep 0.2
echo "StartGame m4" >&3
sleep 0.5

echo "=== 5. Jogador 3 cria SalaRemota (Modo REMOTE) ==="
echo "CreateRoom m5 SalaRemota MaiorMenorExterno" >&5
sleep 0.3

# Captura o RoomID aleatório gerado para a SalaRemota
REMOTE_ROOM_ID=$(grep "Response m5 Success" out_p3.log | awk '{print $4}')
echo "-> SalaRemota criada com RoomID: $REMOTE_ROOM_ID"

echo "=== 6. Jogador 4 entra na SalaRemota e Jogador 3 inicia o jogo REMOTE ==="
echo "JoinRoom m6 $REMOTE_ROOM_ID" >&6
sleep 0.2
echo "StartGame m7" >&5
sleep 0.5

echo "=== 7. Executando jogadas nas duas salas simultaneamente ==="
# Sala LOCAL: Jogador 1 joga 500, Jogador 2 joga 200 (Jogador 1 deve vencer)
echo "PlayerAction m8 500" >&3
echo "PlayerAction m9 200" >&4

# Sala REMOTE: Jogador 3 joga 100, Jogador 4 joga 900 (Jogador 4 deve vencer)
echo "PlayerAction m10 100" >&5
echo "PlayerAction m11 900" >&6
sleep 0.5

echo ""
echo "================ RESULTADOS DOS CLIENTES ================"
echo "--- JOGADOR 1 (Sala LOCAL) ---"
cat out_p1.log
echo "--- JOGADOR 2 (Sala LOCAL) ---"
cat out_p2.log
echo "--- JOGADOR 3 (Sala REMOTE) ---"
cat out_p3.log
echo "--- JOGADOR 4 (Sala REMOTE) ---"
cat out_p4.log
echo "========================================================="