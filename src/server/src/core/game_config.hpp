#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

/// @brief Defines the mode of operation for a game.
enum class GameMode {
    /// @brief The lobby spawns the game binary locally and connects to it.
    LOCAL,
    /// @brief The lobby connects to an externally hosted game process.
    REMOTE
};

/// @brief Holds the configuration parameters for a single game entry.
struct GameConfig {
    /// @brief The name of the game as referenced in the lobby commands.
    std::string name;
    /// @brief The mode of operation for the game.
    GameMode mode;
    /// @brief The target for the game connection.
    std::string target; 
    /// @brief The port for REMOTE mode.
    /// @note 0 (auto-assigned) if LOCAL.
    int port;           
};

/// @brief Loads and parses the game.config file.
class ConfigLoader {
    public:
        /// @brief Reads the configuration file and returns a map of available games.
        /// @param filepath Path to the game.config file.
        /// @return Unordered map mapping the game name to its GameConfig struct.
        /// @note If the file cannot be opened, an empty map is returned.
        static std::unordered_map<std::string, GameConfig> Load(const std::string& filepath);
};