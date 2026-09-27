#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

/// @brief Defines whether the game process is spawned locally by the lobby or hosted externally.
enum class GameMode {
    LOCAL,
    REMOTE
};

/// @brief Holds the configuration parameters for a single game entry.
struct GameConfig {
    std::string name;
    GameMode mode;
    std::string target; // Executable path if LOCAL, IP address if REMOTE
    int port;           // 0 (auto-assigned) if LOCAL, fixed port if REMOTE
};

/// @brief Loads and parses the game.config file.
class ConfigLoader {
public:
    /// @brief Reads the configuration file and returns a map of available games.
    /// @param filepath Path to the game.config file.
    /// @return Unordered map mapping the game name to its GameConfig struct.
    static std::unordered_map<std::string, GameConfig> Load(const std::string& filepath) {
        std::unordered_map<std::string, GameConfig> games;
        std::ifstream file(filepath);

        if (!file.is_open()) {
            std::cerr << "[Config] Warning: Could not open '" << filepath << "'\n";
            return games;
        }

        std::string line;
        int line_num = 0;
        while (std::getline(file, line)) {
            line_num++;
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }

            std::istringstream iss(line);
            std::string name, mode_str, target;

            // Lê os 3 parâmetros comuns a qualquer modo
            if (!(iss >> name >> mode_str >> target)) {
                continue;
            }

            GameConfig config;
            config.name = name;
            config.target = target;
            config.port = 0;

            if (mode_str == "LOCAL") {
                config.mode = GameMode::LOCAL;
                std::cout << "[Config] Loaded game '" << name << "' (LOCAL -> " << target << ")\n";
            } else if (mode_str == "REMOTE") {
                if (!(iss >> config.port)) {
                    std::cerr << "[Config] Error: Missing port for REMOTE game '" << name << "' on line " << line_num << "\n";
                    continue;
                }
                config.mode = GameMode::REMOTE;
                std::cout << "[Config] Loaded game '" << name << "' (REMOTE -> " << target << ":" << config.port << ")\n";
            } else {
                std::cerr << "[Config] Invalid mode '" << mode_str << "' on line " << line_num << "\n";
                continue;
            }

            games[name] = config;
        }

        return games;
    }
};