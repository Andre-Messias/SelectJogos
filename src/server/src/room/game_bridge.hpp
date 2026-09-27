#pragma once

#include <string>
#include <mutex>
#include <atomic>
#include <sys/types.h>

#include "game_config.hpp"

/// @brief Manages the OS process lifecycle (if LOCAL) and the TCP client connection to the Game server.
class GameBridge {
    public:
        /// @brief Constructs a new GameBridge instance.
        /// @param config The GameConfig for the game.
        explicit GameBridge(const GameConfig& config);

        /// @brief Destructor that ensures the game process is terminated and the TCP connection is closed.
        ~GameBridge();

        /// @brief Gets the name of the game associated with this bridge.
        /// @return The name of the game.
        const std::string& GetGameName() const;
        bool IsActive() const { return _is_active.load(); }

        /// @brief Spawns the local binary (if LOCAL) and establishes the TCP connection to the Game.
        bool Connect(int allocated_port);

        /// @brief Closes the TCP socket and terminates the local child process if running.
        /// @return true if an active connection was closed, false if it was already inactive.
        bool Disconnect();

        /// @brief Sends a raw protocol line to the Game server.
        void Send(const std::string& payload);

        /// @brief Reads the next newline-delimited token from the Game server stream.
        std::string ReadNextToken(std::string& buffer);

    private:
        /// @brief The configuration for the game.
        GameConfig _config;
        /// @brief The socket file descriptor for the TCP connection to the Game server; -1 if not connected.
        std::atomic<bool> _is_active;
        /// @brief The socket file descriptor for the TCP connection to the Game server; -1 if not connected.
        int _game_fd;
        /// @brief The process ID of the local game process if it was spawned by this bridge; -1 if no local process is running.
        pid_t _game_pid;
        /// @brief Mutex to protect access to the bridge's state, ensuring thread safety for connection management and communication.
        std::mutex _bridge_mutex;

        /// @brief Spawns a new local process for the game.
        /// @param port The port on which the game will listen.
        /// @return true if the process was successfully spawned, false otherwise.
        bool SpawnLocalProcess(int port);
        
        /// @brief Terminates the local game process if it was spawned by this bridge.
        void StopLocalProcess();
};