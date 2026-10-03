#include <iostream>
#include <string>
#include <csignal>
#include <ctime>

#include "server.hpp"
#include "game.hpp"

/// @brief The main function that initializes the server and starts listening for client connections.
/// @param argc The number of command-line arguments.
/// @param argv The array of command-line arguments.
/// @return 0 if the program runs successfully.
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <PORT>\n";
        return 1;
    }

    // Ignore SIGPIPE to prevent disconnected sockets from terminating the game process
    std::signal(SIGPIPE, SIG_IGN);

    std::srand(std::time(nullptr));

    // Setup server and game
    int port = std::stoi(argv[1]);
    Server server(port);
    Game game;

    // Subscribe to the server's message received event, forwarding messages to the game for processing
    server.OnMessageReceived([&game, &server](int socket_fd, const std::string &token)
                             { game.ProcessMessage(socket_fd, token, server); });

    // When a connection (the Lobby's GameBridge) drops, clean up every player behind it
    server.OnClientDisconnected([&game, &server](int socket_fd)
                                { game.HandleSocketDisconnect(socket_fd, server); });

    // Start the background threads (like inactivity checker)
    game.Start(server);

    // Start the server to listen for incoming connections and handle messages
    server.Start();

    return 0;
}