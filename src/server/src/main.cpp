#include <iostream>
#include <string>
#include <csignal>

#include "network_interface.hpp"

NetworkInterface* global_lobby = nullptr;

void HandleSigInt(int sig) {
    if (global_lobby) {
        std::cout << "\n[System] SIGINT recebido. Encerrando o servidor...\n";
        global_lobby->Stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <LOBBY_PORT> [CONFIG_FILE]\n";
        return 1;
    }

    std::signal(SIGPIPE, SIG_IGN);

    int lobby_port = std::stoi(argv[1]);
    std::string config_file = (argc == 3) ? argv[2] : "game.config";
    
    NetworkInterface lobby(lobby_port, config_file);
    global_lobby = &lobby;
    std::signal(SIGINT, HandleSigInt);
    
    lobby.Start();

    return 0;
}