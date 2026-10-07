#pragma once

#include <string>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <functional>

#include "id_generator.hpp"
#include "room_manager.hpp"

constexpr size_t MAX_NICKNAME_LENGTH = 16;

struct CommandContext
{
    int client_id;
    int socket_fd;
    std::string msg_id;
    std::istringstream &iss;
};

class NetworkInterface
{
public:
    NetworkInterface(int lobby_port, const std::string &config_path = "game.config");

    ~NetworkInterface();

    void Start();

    void Stop();

private:
    using CommandHandler = std::function<void(CommandContext &ctx)>;

    int _port;
    int _server_fd;
    bool _is_running;

    IdGenerator _client_id_gen;
    RoomManager _room_manager;

    std::unordered_map<std::string, CommandHandler> _command_registry;
    std::unordered_map<int, int> _client_sockets;
    std::unordered_map<int, std::string> _client_usernames;
    std::mutex _clients_mutex;

    void HandleClient(int client_fd, int client_id);

    void ProcessMessage(int client_id, int socket_fd, const std::string &message);

    void RegisterCommands();

    // Command Handlers

    void HandleListGames(CommandContext &ctx);

    void HandleListRooms(CommandContext &ctx);

    void HandleSetNick(CommandContext &ctx);

    std::string GetNickname(int client_id);

    void HandleCreateRoom(CommandContext &ctx);

    void HandleJoinRoom(CommandContext &ctx);

    void HandleLeaveRoom(CommandContext &ctx);

    void HandleStartGame(CommandContext &ctx);

    void HandleStopGame(CommandContext &ctx);

    void HandleServerAction(CommandContext &ctx);

    void HandleKickPlayer(CommandContext &ctx);
};