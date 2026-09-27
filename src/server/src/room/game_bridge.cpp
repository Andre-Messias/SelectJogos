#include "game_bridge.hpp"
#include "network_utils.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>

GameBridge::GameBridge(const GameConfig& config)
    : _config(config), _is_active(false), _game_fd(-1), _game_pid(-1) {}

GameBridge::~GameBridge() {
    Disconnect();
}

const std::string& GameBridge::GetGameName() const { return _config.name; }

bool GameBridge::SpawnLocalProcess(int port) {
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[GameBridge] Failed to fork process for game " << _config.name << "\n";
        return false;
    }

    if (pid == 0) {
        for (int fd = 3; fd < 1024; ++fd) {
            close(fd);
        }

        std::string port_str = std::to_string(port);
        execl(_config.target.c_str(), _config.target.c_str(), port_str.c_str(), nullptr);

        std::cerr << "[GameBridge] Failed to execute local binary: " << _config.target << "\n";
        _exit(1);
    }

    _game_pid = pid;
    std::cout << "[GameBridge] Spawned local game '" << _config.name << "' (PID: " << _game_pid << ") on port " << port << "\n";
    return true;
}

void GameBridge::StopLocalProcess() {
    if (_game_pid > 0) {
        kill(_game_pid, SIGTERM);
        waitpid(_game_pid, nullptr, 0);
        std::cout << "[GameBridge] Terminated local game process (PID: " << _game_pid << ")\n";
        _game_pid = -1;
    }
}

bool GameBridge::Connect(int allocated_port) {
    std::lock_guard<std::mutex> lock(_bridge_mutex);

    std::string connect_ip = _config.target;
    int connect_port = _config.port;

    if (_config.mode == GameMode::LOCAL) {
        connect_ip = "127.0.0.1";
        connect_port = allocated_port;
        if (!SpawnLocalProcess(connect_port)) {
            return false;
        }
    }

    int max_attempts = (_config.mode == GameMode::LOCAL) ? 20 : 1;
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

    _is_active.store(true);
    return true;
}

bool GameBridge::Disconnect() {
    std::lock_guard<std::mutex> lock(_bridge_mutex);

    bool was_active = _is_active.exchange(false);
    if (_game_fd != -1) {
        shutdown(_game_fd, SHUT_RDWR);
        close(_game_fd);
        _game_fd = -1;
    }
    StopLocalProcess();
    return was_active;
}

void GameBridge::Send(const std::string& payload) {
    std::lock_guard<std::mutex> lock(_bridge_mutex);
    if (_is_active.load() && _game_fd != -1) {
        NetworkUtils::SendMessage(_game_fd, payload);
    }
}

std::string GameBridge::ReadNextToken(std::string& buffer) {
    int current_fd = _game_fd;
    if (!_is_active.load() || current_fd == -1) {
        return "";
    }
    return NetworkUtils::ReadToken(current_fd, buffer);
}