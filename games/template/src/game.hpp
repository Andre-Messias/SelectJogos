#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <mutex>
#include <unordered_map>
#include <functional>

#include "config.hpp"
#include "player.hpp"
#include "server.hpp"

class Game {
public:
    Game();

    void ProcessMessage(int socket_fd, const std::string& message, Server& server);

private:
    using CommandHandler = std::function<void(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& iss, Server& server)>;

    std::unordered_map<std::string, CommandHandler> _command_registry;
    std::vector<Player> _players;
    int _actions_received;
    std::mutex _game_mutex;

    void RegisterCommands();

    size_t GetConnectedPlayerCount() const;

    void ResetRoundState();

    // Handlers
    void HandleConnectClient(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& /*iss*/, Server& server);

    void HandleDisconnectClient(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& /*iss*/, Server& server);

    void HandleReconnectClient(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& /*iss*/, Server& server);

    void HandlePlayerAction(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& iss, Server& server);

    void HandleResetRound(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& /*iss*/, Server& server);
};