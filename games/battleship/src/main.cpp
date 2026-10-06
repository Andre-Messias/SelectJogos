#include <csignal>
#include <iostream>
#include <string>

#include "game.hpp"
#include "server.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <PORT>\n";
        return 1;
    }
    std::signal(SIGPIPE, SIG_IGN);

    int port = 0;
    try {
        port = std::stoi(argv[1]);
    } catch (...) {
        return 1;
    }
    if (port < 1 || port > 65535) {
        return 1;
    }

    Server server(port);
    Game game;

    // O processo recebe comandos do lobby, não diretamente dos jogadores.
    server.OnMessageReceived([&](int fd, const std::string& line) {
        game.ProcessMessage(fd, line, server);
    });

    server.Start();
    return 0;
}
