#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <memory>
#include <chrono>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>

#include "game_config.hpp"

/// @brief Represents a game room inside the lobby and manages the lifecycle and TCP bridge to the Game process.
class GameRoom : public std::enable_shared_from_this<GameRoom> {
public:
    GameRoom(int id, const std::string& name, const std::string& password, int creator_id, const GameConfig& game_config)
        : _id(id), _name(name), _password(password), _creator_id(creator_id), 
          _game_config(game_config), _is_playing(false), _game_fd(-1), _game_pid(-1) {}

    ~GameRoom() {
        DisconnectGame();
    }

    int GetId() const { return _id; }
    std::string GetName() const { return _name; }
    std::string GetGameName() const { return _game_config.name; }
    bool HasPassword() const { return !_password.empty(); }
    bool CheckPassword(const std::string& pass) const { return _password == pass; }
    bool IsPlaying() const { return _is_playing; }
    bool IsCreator(int client_id) const { return _creator_id == client_id; }
    int GetCreatorId() const { return _creator_id; }

    size_t GetPlayerCount() {
        std::lock_guard<std::mutex> lock(_room_mutex);
        return _clients.size();
    }

    /// @brief Checks if a specific client is currently in the room.
    bool HasClient(int client_id) {
        std::lock_guard<std::mutex> lock(_room_mutex);
        return _clients.find(client_id) != _clients.end();
    }

    void AddClient(int client_id, int socket_fd) {
        std::lock_guard<std::mutex> lock(_room_mutex);
        _clients[client_id] = socket_fd;

        if (_is_playing && _game_fd != -1) {
            SendRawToGame("ConnectClient internal_init " + std::to_string(client_id) + "\n");
        }
    }

    bool RemoveClient(int client_id) {
        std::lock_guard<std::mutex> lock(_room_mutex);
        _clients.erase(client_id);

        if (_creator_id == client_id && !_clients.empty()) {
            _creator_id = _clients.begin()->first;
            return true;
        }
        return false;
    }

    /// @brief Starts the game according to its GameConfig (spawning a local binary or connecting remotely).
    bool StartGame(int allocated_port) {
        std::lock_guard<std::mutex> lock(_room_mutex);

        std::string connect_ip;
        int connect_port;

        if (_game_config.mode == GameMode::LOCAL) {
            connect_ip = "127.0.0.1";
            connect_port = allocated_port;

            pid_t pid = fork();
            if (pid < 0) {
                std::cerr << "[GameRoom] Failed to fork process for game " << _game_config.name << "\n";
                return false;
            } else if (pid == 0) {
                for (int fd = 3; fd < 1024; ++fd) {
                    close(fd);
                }

                std::string port_str = std::to_string(connect_port);
                execl(_game_config.target.c_str(), _game_config.target.c_str(), port_str.c_str(), nullptr);
                
                std::cerr << "[GameRoom] Failed to execute local binary: " << _game_config.target << "\n";
                _exit(1);
            }

            _game_pid = pid;
            std::cout << "[GameRoom] Spawned local game '" << _game_config.name << "' (PID: " << _game_pid << ") on port " << connect_port << "\n";
        } else {
            connect_ip = _game_config.target;
            connect_port = _game_config.port;
        }

        int max_attempts = (_game_config.mode == GameMode::LOCAL) ? 20 : 1;
        bool connected = false;

        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            _game_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (_game_fd < 0) break;

            sockaddr_in game_addr{};
            game_addr.sin_family = AF_INET;
            game_addr.sin_port = htons(connect_port);
            if (inet_pton(AF_INET, connect_ip.c_str(), &game_addr.sin_addr) <= 0) {
                close(_game_fd);
                _game_fd = -1;
                break;
            }

            if (connect(_game_fd, (struct sockaddr*)&game_addr, sizeof(game_addr)) == 0) {
                connected = true;
                break;
            }

            close(_game_fd);
            _game_fd = -1;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (!connected) {
            StopLocalProcess();
            return false;
        }

        _is_playing = true;

        auto self = shared_from_this();
        std::thread([self]() {
            self->ListenToGame();
        }).detach();

        for (const auto& pair : _clients) {
            SendRawToGame("ConnectClient internal_init " + std::to_string(pair.first) + "\n");
        }

        return true;
    }

    /// @brief Injects effective_id into the command and forwards it to the Game process.
    /// @param sender_id The ClientID of the player who sent the message (receives the Response).
    /// @param effective_id The ClientID injected into the Game packet (sender_id for normal actions, 0 for host actions).
    void ForwardCommandToGame(int sender_id, int effective_id, const std::string& command, const std::string& msg_id, const std::string& params) {
        std::lock_guard<std::mutex> lock(_room_mutex);
        if (!_is_playing || _game_fd == -1) return;

        _pending_responses[msg_id] = sender_id;

        std::string payload = command + " " + msg_id + " " + std::to_string(effective_id);
        if (!params.empty()) {
            payload += " " + params;
        }
        payload += "\n";

        SendRawToGame(payload);
    }

    void BroadcastToRoom(const std::string& message) {
        std::lock_guard<std::mutex> lock(_room_mutex);
        for (const auto& pair : _clients) {
            SendToClient(pair.second, message);
        }
    }

    /// @brief Closes the TCP connection and terminates the local child process if applicable.
    /// @return true if an active game was stopped, false if it was already disconnected.
    bool DisconnectGame() {
        std::lock_guard<std::mutex> lock(_room_mutex);
        bool was_playing = _is_playing;
        _is_playing = false;
        _pending_responses.clear();

        if (_game_fd != -1) {
            shutdown(_game_fd, SHUT_RDWR);
            close(_game_fd);
            _game_fd = -1;
        }
        StopLocalProcess();
        return was_playing;
    }

private:
    int _id;
    std::string _name;
    std::string _password;
    int _creator_id;
    GameConfig _game_config;
    bool _is_playing;
    int _game_fd;
    pid_t _game_pid;

    std::unordered_map<int, int> _clients;
    std::unordered_map<std::string, int> _pending_responses;
    std::mutex _room_mutex;

    void StopLocalProcess() {
        if (_game_pid > 0) {
            kill(_game_pid, SIGTERM);
            waitpid(_game_pid, nullptr, 0);
            std::cout << "[GameRoom] Terminated local game process (PID: " << _game_pid << ")\n";
            _game_pid = -1;
        }
    }

    void SendToClient(int client_fd, const std::string& message) {
        write(client_fd, message.c_str(), message.length());
    }

    void SendRawToGame(const std::string& message) {
        if (_game_fd != -1) {
            write(_game_fd, message.c_str(), message.length());
        }
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

    void ListenToGame() {
        std::string buffer = "";
        while (true) {
            int current_fd = _game_fd;
            if (current_fd == -1) break;

            std::string token = ReadToken(current_fd, buffer);
            if (token.empty()) {
                break;
            }

            std::istringstream iss(token);
            std::string type, msg_id;
            iss >> type >> msg_id;

            std::lock_guard<std::mutex> lock(_room_mutex);

            if (type == "Response") {
                if (msg_id == "internal_init") continue;

                auto it = _pending_responses.find(msg_id);
                if (it != _pending_responses.end()) {
                    int target_client_id = it->second;
                    if (_clients.find(target_client_id) != _clients.end()) {
                        SendToClient(_clients[target_client_id], token + "\n");
                    }
                    _pending_responses.erase(it);
                }
            } else {
                for (const auto& pair : _clients) {
                    SendToClient(pair.second, token + "\n");
                }
            }
        }

        // Só envia o broadcast automático se o jogo caiu por conta própria (não via StopGame)
        if (DisconnectGame()) {
            BroadcastToRoom("LogChannel 0 \"Game connection closed\"\n");
        }
    }
};