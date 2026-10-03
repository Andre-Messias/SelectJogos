#include "network_interface.hpp"
#include "network_utils.hpp"

#include <cctype>
#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

NetworkInterface::NetworkInterface(int lobby_port, const std::string& config_path)
    : _port(lobby_port), _server_fd(-1), _is_running(false), _room_manager(config_path) {
    RegisterCommands();
}

NetworkInterface::~NetworkInterface() {
    if (_server_fd != -1) {
        close(_server_fd);
    }
}

void NetworkInterface::Start() {
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[Lobby] Error binding to port " << _port << "\n";
        return;
    }

    listen(_server_fd, 10);
    _is_running = true;
    std::cout << "[Lobby] Running on port " << _port << "...\n";

    while (_is_running) {
        int client_fd = accept(_server_fd, nullptr, nullptr);
        if (client_fd > 0) {
            int assigned_id = _client_id_gen.AcquireId();
            if (assigned_id == -1) {
                std::cerr << "[Lobby] Server full: No ClientIDs available.\n";
                NetworkUtils::SendMessage(client_fd, "LogChannel 0 \"Server is full. Try again later.\"\n");
                close(client_fd);
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(_clients_mutex);
                _client_sockets[assigned_id] = client_fd;
            }

            std::cout << "[Lobby] New connection assigned to ClientID " << assigned_id << "\n";
            NetworkUtils::SendMessage(client_fd, "LogChannel 0 \"Connected with ClientID " + std::to_string(assigned_id) + "\"\n");

            std::thread(&NetworkInterface::HandleClient, this, client_fd, assigned_id).detach();
        }
    }
}

void NetworkInterface::HandleClient(int client_fd, int client_id) {
    std::string buffer;
    while (true) {
        std::string token = NetworkUtils::ReadToken(client_fd, buffer);
        if (token.empty()) {
            break;
        }
        ProcessMessage(client_id, client_fd, token);
    }

    {
        std::lock_guard<std::mutex> lock(_clients_mutex);
        _client_sockets.erase(client_id);
        _client_usernames.erase(client_id);
    }

    _room_manager.RemoveClientFromRoom(client_id);
    _client_id_gen.ReleaseId(client_id);

    close(client_fd);
    std::cout << "[Lobby] ClientID " << client_id << " disconnected.\n";
}

void NetworkInterface::ProcessMessage(int client_id, int socket_fd, const std::string& message) {
    std::istringstream iss(message);
    std::string command, msg_id;

    if (!(iss >> command >> msg_id)) {
        return;
    }

    auto it = _command_registry.find(command);
    if (it != _command_registry.end()) {
        CommandContext ctx{client_id, socket_fd, msg_id, iss};
        it->second(ctx);
        return;
    }

    auto room = _room_manager.GetClientRoom(client_id);
    if (room != nullptr) {
        if (room->IsPlaying()) {
            std::string remaining_params;
            std::getline(iss >> std::ws, remaining_params);
            room->ForwardCommandToGame(client_id, client_id, command, msg_id, remaining_params);
        } else {
            NetworkUtils::SendMessage(socket_fd, "Response " + msg_id + " Fail \"Game has not started yet\"\n");
        }
        return;
    }

    NetworkUtils::SendMessage(socket_fd, "Response " + msg_id + " Fail \"Unknown Lobby Command\"\n");
}

void NetworkInterface::RegisterCommands() {
    _command_registry["ListGames"] = [this](CommandContext& ctx) { HandleListGames(ctx); };
    _command_registry["ListRooms"] = [this](CommandContext& ctx) { HandleListRooms(ctx); };
    _command_registry["SetNick"] = [this](CommandContext& ctx) { HandleSetNick(ctx); };
    _command_registry["CreateRoom"] = [this](CommandContext& ctx) { HandleCreateRoom(ctx); };
    _command_registry["JoinRoom"] = [this](CommandContext& ctx) { HandleJoinRoom(ctx); };
    _command_registry["LeaveRoom"] = [this](CommandContext& ctx) { HandleLeaveRoom(ctx); };
    _command_registry["StartGame"] = [this](CommandContext& ctx) { HandleStartGame(ctx); };
    _command_registry["StopGame"] = [this](CommandContext& ctx) { HandleStopGame(ctx); };
    _command_registry["ServerAction"] = [this](CommandContext& ctx) { HandleServerAction(ctx); };
    _command_registry["KickPlayer"] = [this](CommandContext& ctx) { HandleKickPlayer(ctx); };
}

void NetworkInterface::HandleListGames(CommandContext& ctx) {
    std::string response = "Response " + ctx.msg_id + " Success" + _room_manager.FormatGamesList() + "\n";
    NetworkUtils::SendMessage(ctx.socket_fd, response);
}

void NetworkInterface::HandleListRooms(CommandContext& ctx) {
    std::string response = "Response " + ctx.msg_id + " Success" + _room_manager.FormatRoomsList() + "\n";
    NetworkUtils::SendMessage(ctx.socket_fd, response);
}

/// @brief Nicknames travel inside space-separated protocol lines and quoted LogChannel
/// payloads, so only a conservative ASCII charset is accepted.
static bool IsValidNickname(const std::string& nick) {
    if (nick.empty() || nick.size() > MAX_NICKNAME_LENGTH) {
        return false;
    }
    for (unsigned char c : nick) {
        if (!(std::isalnum(c) || c == '_' || c == '-' || c == '.')) {
            return false;
        }
    }
    return true;
}

std::string NetworkInterface::GetNickname(int client_id) {
    std::lock_guard<std::mutex> lock(_clients_mutex);
    auto it = _client_usernames.find(client_id);
    return (it != _client_usernames.end()) ? it->second : "";
}

void NetworkInterface::HandleSetNick(CommandContext& ctx) {
    std::string new_nick;
    if (!(ctx.iss >> new_nick)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Usage: SetNick <MsgID> <Nickname>\"\n");
        return;
    }

    if (!IsValidNickname(new_nick)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Nickname must be 1-" +
            std::to_string(MAX_NICKNAME_LENGTH) + " chars of letters, digits, '_', '-' or '.'\"\n");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(_clients_mutex);
        for (const auto& pair : _client_usernames) {
            if (pair.first != ctx.client_id && pair.second == new_nick) {
                NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Nickname already in use\"\n");
                return;
            }
        }
        _client_usernames[ctx.client_id] = new_nick;
    }

    // If the client is already inside a room, propagate the new name (and to the game, if running).
    auto room = _room_manager.GetClientRoom(ctx.client_id);
    if (room) {
        room->SetClientName(ctx.client_id, new_nick);
    }

    std::cout << "[Lobby] ClientID " << ctx.client_id << " is now known as " << new_nick << "\n";
    NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Success\n");
    NetworkUtils::SendMessage(ctx.socket_fd, "LogChannel 0 \"Nickname set to " + new_nick + "\"\n");
}

void NetworkInterface::HandleCreateRoom(CommandContext& ctx) {
    std::string room_name, game_name, password;
    if (!(ctx.iss >> room_name >> game_name)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Usage: CreateRoom <MsgID> <RoomName> <GameName> [Password]\"\n");
        return;
    }
    ctx.iss >> password;

    std::string error;
    int room_id = _room_manager.CreateRoom(ctx.client_id, ctx.socket_fd, room_name, game_name, password, error);
    if (room_id == -1) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"" + error + "\"\n");
        return;
    }
    ApplyNicknameToRoom(ctx.client_id);

    NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Success " + std::to_string(room_id) + "\n");
}

void NetworkInterface::HandleJoinRoom(CommandContext& ctx) {
    int room_id;
    std::string password;
    if (!(ctx.iss >> room_id)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Room ID required\"\n");
        return;
    }
    ctx.iss >> password;

    std::string error;
    if (!_room_manager.JoinRoom(ctx.client_id, ctx.socket_fd, room_id, password, error)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"" + error + "\"\n");
        return;
    }
    ApplyNicknameToRoom(ctx.client_id);

    NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Success\n");
}

void NetworkInterface::ApplyNicknameToRoom(int client_id) {
    std::string nick = GetNickname(client_id);
    if (nick.empty()) {
        return;
    }
    auto room = _room_manager.GetClientRoom(client_id);
    if (room) {
        room->SetClientName(client_id, nick);
    }
}

void NetworkInterface::HandleLeaveRoom(CommandContext& ctx) {
    if (!_room_manager.RemoveClientFromRoom(ctx.client_id)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Client is not in a room\"\n");
        return;
    }

    NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Success\n");
}

void NetworkInterface::HandleStartGame(CommandContext& ctx) {
    auto room = _room_manager.GetClientRoom(ctx.client_id);
    if (!room) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Client is not in a room\"\n");
        return;
    }

    if (!room->IsCreator(ctx.client_id)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Only the room creator can start the game\"\n");
        return;
    }

    if (room->IsPlaying()) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Game is already running\"\n");
        return;
    }

    const GameConfig* game_cfg = _room_manager.GetGameConfig(room->GetGameName());
    if (!game_cfg) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Game configuration missing\"\n");
        return;
    }

    int target_port = game_cfg->port;
    if (game_cfg->mode == GameMode::LOCAL) {
        target_port = NetworkUtils::AllocateLocalPort(_port);
        if (target_port == -1) {
            NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"No available local ports to start the game\"\n");
            return;
        }
    }

    if (!room->StartGame(target_port)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Failed to start or connect to game\"\n");
        return;
    }

    NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Success\n");
    room->BroadcastToRoom("LogChannel 0 \"Game " + room->GetGameName() + " started in room " + room->GetName() + "\"\n");
}

void NetworkInterface::HandleStopGame(CommandContext& ctx) {
    auto room = _room_manager.GetClientRoom(ctx.client_id);
    if (!room) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Client is not in a room\"\n");
        return;
    }

    if (!room->IsCreator(ctx.client_id)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Only the room creator can stop the game\"\n");
        return;
    }

    if (!room->IsPlaying()) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"No game is currently running\"\n");
        return;
    }

    room->DisconnectGame();
    std::cout << "[Lobby] Game in room ID " << room->GetId() << " was forcibly stopped by Creator " << ctx.client_id << "\n";
    NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Success\n");
    room->BroadcastToRoom("LogChannel 0 \"Game forcibly stopped by the room creator\"\n");
}

void NetworkInterface::HandleServerAction(CommandContext& ctx) {
    auto room = _room_manager.GetClientRoom(ctx.client_id);
    if (!room) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Client is not in a room\"\n");
        return;
    }

    if (!room->IsCreator(ctx.client_id)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Only the room creator can execute actions as Client 0\"\n");
        return;
    }

    if (!room->IsPlaying()) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Game has not started yet\"\n");
        return;
    }

    std::string game_command;
    if (!(ctx.iss >> game_command)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Usage: ServerAction <MsgID> <GameCommand> [Params...]\"\n");
        return;
    }

    std::string remaining_params;
    std::getline(ctx.iss >> std::ws, remaining_params);

    room->ForwardCommandToGame(ctx.client_id, 0, game_command, ctx.msg_id, remaining_params);
}

void NetworkInterface::HandleKickPlayer(CommandContext& ctx) {
    auto room = _room_manager.GetClientRoom(ctx.client_id);
    if (!room) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Client is not in a room\"\n");
        return;
    }

    if (!room->IsCreator(ctx.client_id)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Only the room creator can kick players\"\n");
        return;
    }

    int target_id;
    if (!(ctx.iss >> target_id)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Usage: KickPlayer <MsgID> <TargetClientID>\"\n");
        return;
    }

    if (target_id == ctx.client_id) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Cannot kick yourself, use LeaveRoom instead\"\n");
        return;
    }

    if (!room->HasClient(target_id)) {
        NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Fail \"Target player is not in this room\"\n");
        return;
    }

    std::string room_name = room->GetName();
    _room_manager.RemoveClientFromRoom(target_id);

    std::cout << "[Lobby] ClientID " << target_id << " was kicked from room ID " << room->GetId() << " by Creator " << ctx.client_id << "\n";
    NetworkUtils::SendMessage(ctx.socket_fd, "Response " + ctx.msg_id + " Success\n");

    int target_fd = -1;
    {
        std::lock_guard<std::mutex> lock(_clients_mutex);
        auto sock_it = _client_sockets.find(target_id);
        if (sock_it != _client_sockets.end()) {
            target_fd = sock_it->second;
        }
    }

    if (target_fd != -1) {
        NetworkUtils::SendMessage(target_fd, "LogChannel 0 \"You have been kicked from room " + room_name + "\"\n");
    }
}