#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <functional>
#include <thread>
#include <random>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "game_config.hpp"
#include "game_room.hpp"

/// @brief Manages the TCP server, game catalog configuration, room lifecycle, and message routing.
class NetworkInterface {
public:
    NetworkInterface(int lobby_port, const std::string& config_path = "game.config") 
        : _port(lobby_port), _server_fd(-1), _is_running(false), 
          _next_local_port(10000), _rng(std::random_device{}()) {
        _available_games = ConfigLoader::Load(config_path);
        RegisterCommands();
    }

    ~NetworkInterface() {
        if (_server_fd != -1) {
            close(_server_fd);
        }
    }

    void Start() {
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
        std::cout << "[Lobby] Running on port " << _port << " with " << _available_games.size() << " configured game(s)...\n";

        while (_is_running) {
            int client_fd = accept(_server_fd, nullptr, nullptr);
            if (client_fd > 0) {
                int assigned_id;
                {
                    std::lock_guard<std::mutex> lock(_lobby_mutex);
                    assigned_id = GenerateClientId();
                    _client_sockets[assigned_id] = client_fd;
                }

                std::cout << "[Lobby] New connection assigned to ClientID " << assigned_id << "\n";
                SendMessage(client_fd, "LogChannel 0 \"Connected with ClientID " + std::to_string(assigned_id) + "\"\n");

                std::thread(&NetworkInterface::HandleClient, this, client_fd, assigned_id).detach();
            }
        }
    }

private:
    using CommandHandler = std::function<void(int client_id, int socket_fd, const std::string& msg_id, std::istringstream& iss)>;

    int _port;
    int _server_fd;
    bool _is_running;
    int _next_local_port;
    std::mt19937 _rng;

    std::unordered_map<std::string, GameConfig> _available_games;
    std::unordered_map<std::string, CommandHandler> _command_registry;
    std::unordered_map<int, int> _client_sockets;
    std::unordered_map<int, std::shared_ptr<GameRoom>> _rooms;
    std::unordered_map<int, int> _client_to_room;
    std::mutex _lobby_mutex;

    int GenerateClientId() {
        std::uniform_int_distribution<int> dist(1000, 999999);
        int candidate_id;
        do {
            candidate_id = dist(_rng);
        } while (_client_sockets.find(candidate_id) != _client_sockets.end());
        return candidate_id;
    }

    int GenerateRoomId() {
        std::uniform_int_distribution<int> dist(1000, 999999);
        int candidate_id;
        do {
            candidate_id = dist(_rng);
        } while (_rooms.find(candidate_id) != _rooms.end());
        return candidate_id;
    }

    int AllocateLocalPort() {
        while (_next_local_port < 65535) {
            int candidate_port = _next_local_port++;
            if (candidate_port == _port) continue;

            int test_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (test_fd < 0) continue;

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(candidate_port);
            addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(test_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                close(test_fd);
                return candidate_port;
            }
            close(test_fd);
        }

        _next_local_port = 10000;
        return AllocateLocalPort();
    }

    void SendMessage(int socket_fd, const std::string& message) {
        write(socket_fd, message.c_str(), message.length());
    }

    std::string ReadToken(int socket_fd, std::string& buffer) {
        size_t pos;
        while ((pos = buffer.find('\n')) == std::string::npos) {
            char temp[256];
            int bytes_read = read(socket_fd, temp, sizeof(temp) - 1);
            if (bytes_read <= 0) {
                if (buffer.empty()) return "";
                std::string last = buffer;
                buffer.clear();
                return last;
            }
            temp[bytes_read] = '\0';
            buffer += temp;
        }
        std::string token = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);
        return token;
    }

    bool RemoveClientFromRoom(int client_id) {
        auto it = _client_to_room.find(client_id);
        if (it == _client_to_room.end()) {
            return false;
        }

        int room_id = it->second;
        _client_to_room.erase(it);

        auto room_it = _rooms.find(room_id);
        if (room_it != _rooms.end()) {
            auto& room = room_it->second;
            bool creator_changed = room->RemoveClient(client_id);

            std::cout << "[Lobby] ClientID " << client_id << " left room ID " << room_id << "\n";

            if (room->GetPlayerCount() == 0) {
                room->DisconnectGame();
                _rooms.erase(room_it);
                std::cout << "[Lobby] Room ID " << room_id << " is empty and was removed.\n";
            } else {
                room->BroadcastToRoom("LogChannel 0 \"ClientID " + std::to_string(client_id) + " left the room\"\n");
                if (creator_changed) {
                    int new_creator = room->GetCreatorId();
                    std::cout << "[Lobby] Room ID " << room_id << " ownership transferred to ClientID " << new_creator << "\n";
                    room->BroadcastToRoom("LogChannel 0 \"ClientID " + std::to_string(new_creator) + " is now the room creator\"\n");
                }
            }
        }
        return true;
    }

    void HandleClient(int client_fd, int client_id) {
        std::string buffer = "";
        while (true) {
            std::string token = ReadToken(client_fd, buffer);
            if (token.empty()) {
                break;
            }
            ProcessMessage(client_id, client_fd, token);
        }

        {
            std::lock_guard<std::mutex> lock(_lobby_mutex);
            _client_sockets.erase(client_id);
            RemoveClientFromRoom(client_id);
        }
        close(client_fd);
        std::cout << "[Lobby] ClientID " << client_id << " disconnected.\n";
    }

    void ProcessMessage(int client_id, int socket_fd, const std::string& message) {
        std::istringstream iss(message);
        std::string command, msg_id;

        if (!(iss >> command >> msg_id)) {
            return;
        }

        std::lock_guard<std::mutex> lock(_lobby_mutex);

        auto it = _command_registry.find(command);
        if (it != _command_registry.end()) {
            it->second(client_id, socket_fd, msg_id, iss);
            return;
        }

        auto room_it = _client_to_room.find(client_id);
        if (room_it != _client_to_room.end()) {
            auto& room = _rooms[room_it->second];
            if (room->IsPlaying()) {
                std::string remaining_params;
                std::getline(iss >> std::ws, remaining_params);
                // Jogador comum: sender_id e effective_id são o próprio client_id
                room->ForwardCommandToGame(client_id, client_id, command, msg_id, remaining_params);
                return;
            } else {
                SendMessage(socket_fd, "Response " + msg_id + " Fail \"Game has not started yet\"\n");
                return;
            }
        }

        SendMessage(socket_fd, "Response " + msg_id + " Fail \"Unknown Lobby Command\"\n");
    }

    void RegisterCommands() {
        _command_registry["ListGames"] = [this](int cid, int fd, const std::string& mid, std::istringstream& iss) {
            this->HandleListGames(cid, fd, mid, iss);
        };
        _command_registry["ListRooms"] = [this](int cid, int fd, const std::string& mid, std::istringstream& iss) {
            this->HandleListRooms(cid, fd, mid, iss);
        };
        _command_registry["CreateRoom"] = [this](int cid, int fd, const std::string& mid, std::istringstream& iss) {
            this->HandleCreateRoom(cid, fd, mid, iss);
        };
        _command_registry["JoinRoom"] = [this](int cid, int fd, const std::string& mid, std::istringstream& iss) {
            this->HandleJoinRoom(cid, fd, mid, iss);
        };
        _command_registry["LeaveRoom"] = [this](int cid, int fd, const std::string& mid, std::istringstream& iss) {
            this->HandleLeaveRoom(cid, fd, mid, iss);
        };
        _command_registry["StartGame"] = [this](int cid, int fd, const std::string& mid, std::istringstream& iss) {
            this->HandleStartGame(cid, fd, mid, iss);
        };
        _command_registry["StopGame"] = [this](int cid, int fd, const std::string& mid, std::istringstream& iss) {
            this->HandleStopGame(cid, fd, mid, iss);
        };
        _command_registry["ServerAction"] = [this](int cid, int fd, const std::string& mid, std::istringstream& iss) {
            this->HandleServerAction(cid, fd, mid, iss);
        };
        _command_registry["KickPlayer"] = [this](int cid, int fd, const std::string& mid, std::istringstream& iss) {
            this->HandleKickPlayer(cid, fd, mid, iss);
        };
    }

    // --- Command Handlers ---

    void HandleListGames(int /*client_id*/, int socket_fd, const std::string& msg_id, std::istringstream& /*iss*/) {
        std::string response = "Response " + msg_id + " Success";
        if (_available_games.empty()) {
            response += " \"No games configured\"";
        } else {
            for (const auto& pair : _available_games) {
                std::string mode_str = (pair.second.mode == GameMode::LOCAL) ? "[Local]" : "[Remote]";
                response += " | " + pair.first + mode_str;
            }
        }
        response += "\n";
        SendMessage(socket_fd, response);
    }

    void HandleListRooms(int /*client_id*/, int socket_fd, const std::string& msg_id, std::istringstream& /*iss*/) {
        std::string response = "Response " + msg_id + " Success";
        
        if (_rooms.empty()) {
            response += " \"No rooms available\"";
        } else {
            for (const auto& pair : _rooms) {
                auto& room = pair.second;
                std::string privacy = room->HasPassword() ? "[Private]" : "[Public]";
                std::string status = room->IsPlaying() ? "[Playing]" : "[Waiting]";
                
                response += " | ID:" + std::to_string(room->GetId()) + 
                            " Name:" + room->GetName() + 
                            " Game:" + room->GetGameName() +
                            " Players:" + std::to_string(room->GetPlayerCount()) + 
                            " " + privacy + status;
            }
        }
        response += "\n";
        SendMessage(socket_fd, response);
    }

    void HandleCreateRoom(int client_id, int socket_fd, const std::string& msg_id, std::istringstream& iss) {
        if (_client_to_room.find(client_id) != _client_to_room.end()) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Client already in a room\"\n");
            return;
        }

        std::string room_name, game_name, password = "";
        if (!(iss >> room_name >> game_name)) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Usage: CreateRoom <MsgID> <RoomName> <GameName> [Password]\"\n");
            return;
        }
        iss >> password;

        auto game_it = _available_games.find(game_name);
        if (game_it == _available_games.end()) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Game not found in game.config\"\n");
            return;
        }

        int room_id = GenerateRoomId();
        auto new_room = std::make_shared<GameRoom>(room_id, room_name, password, client_id, game_it->second);
        new_room->AddClient(client_id, socket_fd);

        _rooms[room_id] = new_room;
        _client_to_room[client_id] = room_id;

        std::cout << "[Lobby] ClientID " << client_id << " created room '" << room_name 
                  << "' for game '" << game_name << "' (ID: " << room_id << ")\n";
        SendMessage(socket_fd, "Response " + msg_id + " Success " + std::to_string(room_id) + "\n");
    }

    void HandleJoinRoom(int client_id, int socket_fd, const std::string& msg_id, std::istringstream& iss) {
        if (_client_to_room.find(client_id) != _client_to_room.end()) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Client already in a room\"\n");
            return;
        }

        int room_id;
        std::string password = "";
        if (!(iss >> room_id)) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Room ID required\"\n");
            return;
        }
        iss >> password;

        auto it = _rooms.find(room_id);
        if (it == _rooms.end()) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Room not found\"\n");
            return;
        }

        auto& room = it->second;
        if (room->HasPassword() && !room->CheckPassword(password)) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Incorrect password\"\n");
            return;
        }

        room->AddClient(client_id, socket_fd);
        _client_to_room[client_id] = room_id;

        std::cout << "[Lobby] ClientID " << client_id << " joined room ID " << room_id << "\n";
        SendMessage(socket_fd, "Response " + msg_id + " Success\n");
        room->BroadcastToRoom("LogChannel 0 \"ClientID " + std::to_string(client_id) + " joined the room\"\n");
    }

    void HandleLeaveRoom(int client_id, int socket_fd, const std::string& msg_id, std::istringstream& /*iss*/) {
        if (!RemoveClientFromRoom(client_id)) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Client is not in a room\"\n");
            return;
        }

        SendMessage(socket_fd, "Response " + msg_id + " Success\n");
    }

    void HandleStartGame(int client_id, int socket_fd, const std::string& msg_id, std::istringstream& /*iss*/) {
        auto it = _client_to_room.find(client_id);
        if (it == _client_to_room.end()) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Client is not in a room\"\n");
            return;
        }

        auto& room = _rooms[it->second];
        if (!room->IsCreator(client_id)) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Only the room creator can start the game\"\n");
            return;
        }

        if (room->IsPlaying()) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Game is already running\"\n");
            return;
        }

        const auto& game_cfg = _available_games[room->GetGameName()];
        int target_port = (game_cfg.mode == GameMode::LOCAL) ? AllocateLocalPort() : game_cfg.port;

        if (!room->StartGame(target_port)) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Failed to start or connect to game\"\n");
            return;
        }

        SendMessage(socket_fd, "Response " + msg_id + " Success\n");
        room->BroadcastToRoom("LogChannel 0 \"Game " + room->GetGameName() + " started in room " + room->GetName() + "\"\n");
    }

    void HandleStopGame(int client_id, int socket_fd, const std::string& msg_id, std::istringstream& /*iss*/) {
        auto it = _client_to_room.find(client_id);
        if (it == _client_to_room.end()) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Client is not in a room\"\n");
            return;
        }

        auto& room = _rooms[it->second];
        if (!room->IsCreator(client_id)) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Only the room creator can stop the game\"\n");
            return;
        }

        if (!room->IsPlaying()) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"No game is currently running\"\n");
            return;
        }

        room->DisconnectGame();
        std::cout << "[Lobby] Game in room ID " << room->GetId() << " was forcibly stopped by Creator " << client_id << "\n";
        SendMessage(socket_fd, "Response " + msg_id + " Success\n");
        room->BroadcastToRoom("LogChannel 0 \"Game forcibly stopped by the room creator\"\n");
    }

    void HandleServerAction(int client_id, int socket_fd, const std::string& msg_id, std::istringstream& iss) {
        auto it = _client_to_room.find(client_id);
        if (it == _client_to_room.end()) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Client is not in a room\"\n");
            return;
        }

        auto& room = _rooms[it->second];
        if (!room->IsCreator(client_id)) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Only the room creator can execute actions as Client 0\"\n");
            return;
        }

        if (!room->IsPlaying()) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Game has not started yet\"\n");
            return;
        }

        std::string game_command;
        if (!(iss >> game_command)) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Usage: ServerAction <MsgID> <GameCommand> [Params...]\"\n");
            return;
        }

        std::string remaining_params;
        std::getline(iss >> std::ws, remaining_params);

        // Envia para o Game injetando effective_id = 0, mas mantém sender_id = client_id para devolver o Response
        room->ForwardCommandToGame(client_id, 0, game_command, msg_id, remaining_params);
    }

    void HandleKickPlayer(int client_id, int socket_fd, const std::string& msg_id, std::istringstream& iss) {
        auto it = _client_to_room.find(client_id);
        if (it == _client_to_room.end()) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Client is not in a room\"\n");
            return;
        }

        auto& room = _rooms[it->second];
        if (!room->IsCreator(client_id)) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Only the room creator can kick players\"\n");
            return;
        }

        int target_id;
        if (!(iss >> target_id)) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Usage: KickPlayer <MsgID> <TargetClientID>\"\n");
            return;
        }

        if (target_id == client_id) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Cannot kick yourself, use LeaveRoom instead\"\n");
            return;
        }

        if (!room->HasClient(target_id)) {
            SendMessage(socket_fd, "Response " + msg_id + " Fail \"Target player is not in this room\"\n");
            return;
        }

        room->RemoveClient(target_id);
        _client_to_room.erase(target_id);

        std::cout << "[Lobby] ClientID " << target_id << " was kicked from room ID " << room->GetId() << " by Creator " << client_id << "\n";
        SendMessage(socket_fd, "Response " + msg_id + " Success\n");

        if (_client_sockets.find(target_id) != _client_sockets.end()) {
            SendMessage(_client_sockets[target_id], "LogChannel 0 \"You have been kicked from room " + room->GetName() + "\"\n");
        }
        room->BroadcastToRoom("LogChannel 0 \"ClientID " + std::to_string(target_id) + " was kicked from the room\"\n");
    }
};