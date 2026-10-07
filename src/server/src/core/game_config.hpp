#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

enum class GameMode
{
    LOCAL,
    REMOTE
};

struct GameConfig
{
    std::string name;
    GameMode mode;
    std::string target;
    int port;
};

class ConfigLoader
{
public:
    static std::unordered_map<std::string, GameConfig> Load(const std::string &filepath);
};