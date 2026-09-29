#include "game.hpp"

Game::Game() : _actions_received(0) {
    RegisterCommands();
}

void Game::ProcessMessage(int socket_fd, const std::string& message, Server& server) {
    std::istringstream iss(message);
    std::string command, msg_id;
    int client_id;

    // Extract the base protocol header
    if (!(iss >> command >> msg_id >> client_id)) {
        return;
    }

    // Lock the game state to ensure thread safety
    std::lock_guard<std::mutex> lock(_game_mutex);

    // Dispatch the command
    auto it = _command_registry.find(command);
    if (it != _command_registry.end()) {
        it->second(socket_fd, msg_id, client_id, iss, server);
    } else {
        std::cout << "[Game] Unknown command: " << command << "\n";
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Unknown Command\"\n");
    }
}

void Game::RegisterCommands() {
    _command_registry["ConnectClient"] = [this](int fd, const std::string& mid, int cid, std::istringstream& iss, Server& srv) {
        this->HandleConnectClient(fd, mid, cid, iss, srv);
    };
    
    _command_registry["PlayerAction"] = [this](int fd, const std::string& mid, int cid, std::istringstream& iss, Server& srv) {
        this->HandlePlayerAction(fd, mid, cid, iss, srv);
    };
}

void Game::HandleConnectClient(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& /*iss*/, Server& server) {
    bool exists = false;
    for (auto& p : _players) {
        if (p.getId() == (uint)client_id) exists = true;
    }
    if (!exists) _players.push_back(Player(client_id));

    std::cout << "[Game] Jogador " << client_id << " conectou.\n";
    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
}

void Game::HandlePlayerAction(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& iss, Server& server) {
    std::string action_value;
    iss >> action_value;

    try {
        int number = std::stoi(action_value);
        bool player_found = false;

        for (auto& p : _players) {
            if (p.getId() == (uint)client_id) {
                p.setNumber(number);
                player_found = true;
                _actions_received++;
                break;
            }
        }

        if (player_found) {
            std::cout << "[Game] Jogador " << client_id << " jogou: " << number << "\n";
            server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");

            if (_actions_received == 2) {
                std::string resultado;
                if (_players[0].getNumber() > _players[1].getNumber()) {
                    resultado = "Player " + std::to_string(_players[0].getId()) + " venceu!";
                } else if (_players[0].getNumber() < _players[1].getNumber()) {
                    resultado = "Player " + std::to_string(_players[1].getId()) + " venceu!";
                } else {
                    resultado = "Deu empate!";
                }
                std::cout << "[Game] Partida encerrada: " << resultado << "\n";
                server.Broadcast("LogChannel All \"" + resultado + "\"\n");
            }
        } else {
            server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogador Nao Conectado\"\n");
        }
    } catch (...) {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Acao Invalida\"\n");
    }
}