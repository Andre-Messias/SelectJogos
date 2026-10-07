#include <iostream>
#include <string>
#include <csignal>
#include <ctime>

#include "server.hpp"
#include "game.hpp"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <PORT>\n";
        return 1;
    }

    // Evita crash se tentar enviar pacote para um socket que já fechou do outro lado
    std::signal(SIGPIPE, SIG_IGN);

    std::srand(std::time(nullptr));

    int port = std::stoi(argv[1]);
    Server server(port);
    Game game;

    server.OnMessageReceived([&game, &server](int socket_fd, const std::string &token)
                             { game.ProcessMessage(socket_fd, token, server); });

    server.OnClientDisconnected([&game, &server](int socket_fd)
                                { game.HandleSocketDisconnect(socket_fd, server); });

    game.Start(server);
    server.Start();

    return 0;
}