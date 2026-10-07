#include <iostream>
#include <string>
#include <csignal>
#include "client_state.hpp"
#include "network_client.hpp"
#include "protocol_parser.hpp"
#include "terminal_ui.hpp"

TerminalUI* global_ui = nullptr;
NetworkClient* global_network = nullptr;

void HandleSigInt(int sig) {
    if (global_network) {
        global_network->Disconnect();
    }
    if (global_ui) {
        global_ui->DisableRawMode();
        std::cout << "\n[!] Conexão encerrada pelo usuário (Ctrl+C).\n";
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <LOBBY_IP> <LOBBY_PORT> [HELP_FILE]\n";
        return 1;
    }

    // Ignore SIGPIPE so broken sockets never crash the client UI
    std::signal(SIGPIPE, SIG_IGN);

    std::string ip = argv[1];
    int port = std::stoi(argv[2]);
    std::string help_file = (argc == 4) ? argv[3] : "help.txt";

    ClientState state;
    NetworkClient network;
    ProtocolParser parser(state, network, help_file);
    TerminalUI ui(state);

    // Wire network incoming lines to the protocol parser
    network.SetMessageCallback([&parser, &ui](const std::string& line) {
        parser.HandleServerMessage(line);
        ui.RefreshScreen();
    });

    // Wire disconnection event
    network.SetDisconnectCallback([&state, &ui]() {
        state.SetAlert("[!] Connection lost with Lobby server.");
        state.AddLog("[System] Disconnected from Lobby.");
        ui.RefreshScreen();
    });

    // Wire UI command submission to network protocol parser
    ui.SetCommandCallback([&parser, &ui](const std::string& cmd) {
        parser.HandleLocalInput(cmd);
        ui.RefreshScreen();
    });

    // Connect to Lobby server
    state.AddLog("[System] Connecting to " + ip + ":" + std::to_string(port) + "...");
    state.AddLog("[System] Type 'help' (or 'h') to see available commands, or 'exit' to quit.");
    if (!network.Connect(ip, port)) {
        std::cerr << "Error: Could not connect to Lobby at " << ip << ":" << port << "\n";
        return 1;
    }

    // Register SIGINT
    global_ui = &ui;
    global_network = &network;
    std::signal(SIGINT, HandleSigInt);

    // Start TUI event loop (blocking until exit)
    ui.Run();

    network.Disconnect();
    return 0;
}