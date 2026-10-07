#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <mutex>
#include <unordered_map>
#include <functional>

#include "player.hpp"
#include "server.hpp"
#include "board.hpp"
#include "room.hpp"

class Game
{
public:
    Game();

    ~Game();

    void Start(Server &server);

    void ProcessMessage(int socket_fd, const std::string &message, Server &server);

    void HandleSocketDisconnect(int socket_fd, Server &server);

private:
    using CommandHandler = std::function<void(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server)>;

    std::unordered_map<std::string, CommandHandler> _command_registry;
    std::vector<Player> _players;
    std::mutex _game_mutex;

    int _active_room_id = 1;
    std::vector<Room> _rooms;

    void InitRooms();

    void RegisterCommands();

    void InactivityCheckerLoop(Server &server);

    std::thread _inactivity_thread;
    std::atomic<bool> _is_running;

    std::string GetRoomPlayersString(int room_id);

    void BroadcastRoomScreen(Room &room, Server &server);

    Room *FindRoom(int room_id);

    Player *FindPlayer(int client_id);

    void DetachPlayerFromRoom(Player &p, Server &server);

    // Handlers
    void HandleConnectClient(int socket_fd, const std::string &msg_id, int client_id, std::istringstream & /*iss*/, Server &server);

    void HandleDisconnectClient(int socket_fd, const std::string &msg_id, int client_id, std::istringstream & /*iss*/, Server &server);

    void HandleReconnectClient(int socket_fd, const std::string &msg_id, int client_id, std::istringstream & /*iss*/, Server &server);

    void HandleSetPlayerName(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server);

    void HandleJoinRoom(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server);

    void HandlePlayerAction(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server);

    void HandleStartGame(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server);

    void HandleNameTeam(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server);

    void HandleRanking(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server);

    void BroadcastToRoom(int room_id, const std::string &message, Server &server);

    void SendToPlayer(int client_id, const std::string &message, Server &server);
};