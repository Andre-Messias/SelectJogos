#include "game.hpp"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <unistd.h>

void Game::InactivityCheckerLoop(Server & /*server*/)
{
    // Inactivity kicks are disabled: player departures are reported by the Lobby
    // (DisconnectClient) and lost Lobby connections by HandleSocketDisconnect.
    while (_is_running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

void Game::DetachPlayerFromRoom(Player &p, Server &server)
{
    int room_id = p.getRoomId();
    p.setRoomId(-1);
    if (room_id == -1)
        return;

    Room *room = FindRoom(room_id);
    if (!room)
        return;

    bool is_empty = true;
    for (auto &other : _players)
    {
        if (other.isConnected() && other.getRoomId() == room_id)
        {
            is_empty = false;
            break;
        }
    }

    if (is_empty)
        room->Reset();
    else
        BroadcastRoomScreen(*room, server);
}

void Game::HandleConnectClient(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server)
{
    // Payload from the Lobby: ConnectClient <MsgID> <ClientID> <LobbyRoomID> [Nickname]
    int lobby_room_id = -1;
    std::string nickname;
    iss >> lobby_room_id >> nickname;

    Player *player = nullptr;
    for (auto &p : _players)
    {
        if (p.getId() == client_id)
        {
            player = &p;
            break;
        }
    }

    if (player && player->isConnected())
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogador já está online\"\n");
        return;
    }

    bool returning = (player != nullptr);
    if (!player)
    {
        _players.emplace_back(client_id, socket_fd);
        player = &_players.back();
    }

    player->setConnected(true);
    player->setSocketFd(socket_fd);
    if (nickname.empty())
        player->resetName();
    else
        player->setName(nickname);
    player->setRoomId(_active_room_id); // Every player joins the currently active difficulty
    player->updateActivity();

    std::cout << "[Game] " << player->getName() << " (" << client_id << ") " << (returning ? "reconectado" : "entrou") << ".\n";
    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
    BroadcastToRoom(_active_room_id, player->getName() + (returning ? " voltou." : " entrou no servidor.") + "\n", server);

    if (Room *room = FindRoom(_active_room_id))
        BroadcastRoomScreen(*room, server);
}

void Game::HandleDisconnectClient(int socket_fd, const std::string &msg_id, int client_id, std::istringstream & /*iss*/, Server &server)
{
    for (auto &p : _players)
    {
        if (p.getId() == client_id)
        {
            p.setConnected(false);
            DetachPlayerFromRoom(p, server);
            server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
            std::cout << "[Game] " << p.getName() << " (" << client_id << ") desconectado.\n";
            return;
        }
    }
    server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogador não encontrado\"\n");
}

void Game::HandleReconnectClient(int socket_fd, const std::string &msg_id, int client_id, std::istringstream & /*iss*/, Server &server)
{
    Player *player = nullptr;
    for (auto &p : _players)
    {
        if (p.getId() == client_id)
        {
            player = &p;
            break;
        }
    }

    if (!player)
    {
        _players.emplace_back(client_id, socket_fd);
        player = &_players.back();
    }

    // Previously the room was never restored here, so a player who did `lv` + `jr`
    // was "back" but could not see the board or play ("Entre em uma sala primeiro").
    player->setConnected(true);
    player->setSocketFd(socket_fd);
    player->setRoomId(_active_room_id);
    player->updateActivity();

    std::cout << "[Game] " << player->getName() << " (" << client_id << ") reconectado.\n";
    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
    BroadcastToRoom(_active_room_id, player->getName() + " voltou.\n", server);

    if (Room *room = FindRoom(_active_room_id))
        BroadcastRoomScreen(*room, server);
}

void Game::HandleSetPlayerName(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server)
{
    // Payload from the Lobby: SetPlayerName <MsgID> <ClientID> <Nickname>
    std::string nickname;
    if (!(iss >> nickname))
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Nome vazio\"\n");
        return;
    }

    for (auto &p : _players)
    {
        if (p.getId() != client_id)
            continue;

        std::string old_name = p.getName();
        p.setName(nickname);
        server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
        if (old_name == p.getName())
            return;

        std::cout << "[Game] " << old_name << " agora é " << p.getName() << ".\n";
        if (Room *room = FindRoom(p.getRoomId()))
        {
            BroadcastToRoom(room->GetId(), old_name + " agora é " + p.getName() + ".\n", server);
            BroadcastRoomScreen(*room, server); // Refresh the header with the new name
        }
        return;
    }

    server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogador não encontrado\"\n");
}

void Game::HandleSocketDisconnect(int socket_fd, Server &server)
{
    {
        std::lock_guard<std::mutex> lock(_game_mutex);

        // All players behind this connection share its fd (they are multiplexed by the Lobby's GameBridge).
        int dropped = 0;
        for (auto &p : _players)
        {
            if (p.isConnected() && p.getSocketFd() == socket_fd)
            {
                p.setConnected(false);
                p.setSocketFd(-1);
                DetachPlayerFromRoom(p, server);
                dropped++;
            }
        }
        std::cout << "[Game] Conexão com a Lobby encerrada (" << dropped << " jogador(es) removido(s)).\n";
    }

    // A LOCAL game is fork+exec'd by the Lobby. If the Lobby died without cleaning up (e.g. pkill),
    // this process is re-parented to PID 1 and would otherwise block in accept() forever.
    // Games started manually (REMOTE mode) keep their shell as parent and keep running.
    if (getppid() == 1)
    {
        std::cout << "[Game] Lobby que iniciou este processo não existe mais. Encerrando.\n";
        std::exit(0);
    }
}
