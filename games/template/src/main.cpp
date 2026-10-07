#include <iostream>
#include <string>
#include <csignal>

#include "server.hpp"
#include "game.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <PORT>\n";
        return 1;
    }

    // Ignore SIGPIPE to prevent disconnected sockets from terminating the game process
    std::signal(SIGPIPE, SIG_IGN);

    // Setup server and game
    int port = std::stoi(argv[1]);
    Server server(port);
    Game game;

    // Subscribe to the server's message received event, forwarding messages to the game for processing
    server.OnMessageReceived([&game, &server](int socket_fd, const std::string& token) {
        game.ProcessMessage(socket_fd, token, server);
    });

    // Start the server to listen for incoming connections and handle messages
    server.Start();

    return 0;
}