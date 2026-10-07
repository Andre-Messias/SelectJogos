#include "game_config.hpp"

std::unordered_map<std::string, GameConfig> ConfigLoader::Load(const std::string &filepath)
{
    std::unordered_map<std::string, GameConfig> games;
    std::ifstream file(filepath);

    if (!file.is_open())
    {
        std::cerr << "[Config] Warning: Could not open '" << filepath << "'\n";
        return games;
    }

    std::string line;
    int line_num = 0;
    while (std::getline(file, line))
    {
        line_num++;

        // Remove comments and trim whitespace
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos)
        {
            line = line.substr(0, comment_pos);
        }

        std::istringstream iss(line);
        std::string name, mode_str, target;

        if (!(iss >> name >> mode_str >> target))
        {
            continue;
        }

        GameConfig config;
        config.name = name;
        config.target = target;
        config.port = 0;

        if (mode_str == "LOCAL")
        {
            config.mode = GameMode::LOCAL;
            std::cout << "[Config] Loaded game '" << name << "' (LOCAL -> " << target << ")\n";
        }
        else if (mode_str == "REMOTE")
        {
            if (!(iss >> config.port))
            {
                std::cerr << "[Config] Error: Missing port for REMOTE game '" << name << "' on line " << line_num << "\n";
                continue;
            }
            config.mode = GameMode::REMOTE;
            std::cout << "[Config] Loaded game '" << name << "' (REMOTE -> " << target << ":" << config.port << ")\n";
        }
        else
        {
            std::cerr << "[Config] Invalid mode '" << mode_str << "' on line " << line_num << "\n";
            continue;
        }

        games[name] = config;
    }
    file.close();

    return games;
}