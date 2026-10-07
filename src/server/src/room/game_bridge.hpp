#pragma once

#include <string>
#include <mutex>
#include <atomic>
#include <sys/types.h>

#include "game_config.hpp"

class GameBridge
{
public:
    explicit GameBridge(const GameConfig &config);

    ~GameBridge();

    const std::string &GetGameName() const;

    bool IsActive() const { return _is_active.load(); }

    bool Connect(int allocated_port);

    bool Disconnect();

    void Send(const std::string &payload);

    std::string ReadNextToken(std::string &buffer);

private:
    GameConfig _config;
    std::atomic<bool> _is_active;
    std::atomic<int> _game_fd;
    pid_t _game_pid;
    std::mutex _bridge_mutex;

    bool SpawnLocalProcess(int port);

    void StopLocalProcess();
};