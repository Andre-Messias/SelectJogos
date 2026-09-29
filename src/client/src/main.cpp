#include <iostream>
#include <string>
#include <csignal>
#include "client_state.hpp"
#include "network_client.hpp"
#include "protocol_parser.hpp"
#include "terminal_ui.hpp"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <LOBBY_IP> <LOBBY_PORT>\n";
        return 1;
    }

    // Ignore SIGPIPE so broken sockets never crash the client UI
    std::signal(SIGPIPE, SIG_IGN);

    std::string ip = argv[1];
    int port = std::stoi(argv[2]);

    ClientState state;
    NetworkClient network;
    ProtocolParser parser(state, network);
    TerminalUI ui(state);

    // Wire network incoming lines to the protocol parser
    network.SetMessageCallback([&parser, &ui](const std::string& line) {
        parser.HandleServerMessage(line);
        ui.RefreshScreen();
    });

    // Wire disconnection event
    network.SetDisconnectCallback([&state, &ui]() {
        state.SetAlert("[!] Conexão perdida com o servidor Lobby.");
        state.AddLog("[Sistema] Desconectado do Lobby.");
        ui.RefreshScreen();
    });

    // Wire UI command submission to network protocol parser
    ui.SetCommandCallback([&parser, &ui](const std::string& cmd) {
        parser.HandleLocalInput(cmd);
        ui.RefreshScreen();
    });

    // Connect to Lobby server
    state.AddLog("[Sistema] Conectando a " + ip + ":" + std::to_string(port) + "...");
    if (!network.Connect(ip, port)) {
        std::cerr << "Erro: Não foi possível conectar ao Lobby em " << ip << ":" << port << "\n";
        return 1;
    }

    // Start TUI event loop (blocking until exit)
    ui.Run();

    network.Disconnect();
    return 0;
}