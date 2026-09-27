#include <iostream>
#include <string>

#include "network_interface.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <LOBBY_PORT> [CONFIG_FILE]\n";
        return 1;
    }

    int lobby_port = std::stoi(argv[1]);
    std::string config_file = (argc == 3) ? argv[2] : "game.config";
    
    NetworkInterface lobby(lobby_port, config_file);
    lobby.Start();

    return 0;
}