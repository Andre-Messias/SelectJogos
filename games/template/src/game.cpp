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

    _command_registry["DisconnectClient"] = [this](int fd, const std::string& mid, int cid, std::istringstream& iss, Server& srv) {
        this->HandleDisconnectClient(fd, mid, cid, iss, srv);
    };

    _command_registry["ReconnectClient"] = [this](int fd, const std::string& mid, int cid, std::istringstream& iss, Server& srv) {
        this->HandleReconnectClient(fd, mid, cid, iss, srv);
    };
    
    _command_registry["PlayerAction"] = [this](int fd, const std::string& mid, int cid, std::istringstream& iss, Server& srv) {
        this->HandlePlayerAction(fd, mid, cid, iss, srv);
    };

    _command_registry["ResetRound"] = [this](int fd, const std::string& mid, int cid, std::istringstream& iss, Server& srv) {
        this->HandleResetRound(fd, mid, cid, iss, srv);
    };
}

size_t Game::GetConnectedPlayerCount() const {
    size_t count = 0;
    for (const auto& p : _players) {
        if (p.isConnected()) {
            count++;
        }
    }
    return count;
}

void Game::ResetRoundState() {
    _actions_received = 0;
    for (auto& p : _players) {
        p.resetRound();
    }
}

void Game::HandleConnectClient(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& /*iss*/, Server& server) {
    Player* existing_player = nullptr;
    for (auto& p : _players) {
        if (p.getId() == client_id) {
            existing_player = &p;
            break;
        }
    }

    // If the player is already connected, simply acknowledge success
    if (existing_player && existing_player->isConnected()) {
        server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
        return;
    }

    // Refuse connection if the room already has the maximum number of active players (2)
    if (GetConnectedPlayerCount() >= REQUIRED_PLAYERS) {
        std::cout << "[Game] Connection refused for player " << client_id << ": max limit of " << REQUIRED_PLAYERS << " players reached.\n";
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Room is full: game supports at most 2 players\"\n");
        server.SendMessage(socket_fd, "LogChannel " + std::to_string(client_id) + " \"Connection refused by game: match already has 2 players.\"\n");
        return;
    }

    if (existing_player) {
        existing_player->setConnected(true);
    } else {
        _players.emplace_back(client_id);
    }

    std::cout << "[Game] Player " << client_id << " connected.\n";
    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
}

void Game::HandleDisconnectClient(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& /*iss*/, Server& server) {
    for (auto& p : _players) {
        if (p.getId() == client_id && p.isConnected()) {
            if (p.hasPlayed() && _actions_received > 0) {
                _actions_received--;
            }
            p.resetRound();
            p.setConnected(false);
            break;
        }
    }

    std::cout << "[Game] Player " << client_id << " disconnected.\n";
    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
    server.SendMessage(socket_fd, "LogChannel All \"Player " + std::to_string(client_id) + " left the match.\"\n");
}

void Game::HandleReconnectClient(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& /*iss*/, Server& server) {
    Player* existing_player = nullptr;
    for (auto& p : _players) {
        if (p.getId() == client_id) {
            existing_player = &p;
            break;
        }
    }

    if (existing_player && existing_player->isConnected()) {
        server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
        return;
    }

    // Refuse reconnection if 2 active players are already in the match
    if (GetConnectedPlayerCount() >= REQUIRED_PLAYERS) {
        std::cout << "[Game] Reconnection refused for player " << client_id << ": max limit of " << REQUIRED_PLAYERS << " players reached.\n";
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Room is full: game supports at most 2 players\"\n");
        server.SendMessage(socket_fd, "LogChannel " + std::to_string(client_id) + " \"Reconnection refused by game: match already has 2 players.\"\n");
        return;
    }

    if (existing_player) {
        existing_player->setConnected(true);
    } else {
        _players.emplace_back(client_id);
    }

    std::cout << "[Game] Player " << client_id << " reconnected.\n";
    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
    server.SendMessage(socket_fd, "LogChannel All \"Player " + std::to_string(client_id) + " returned to the match.\"\n");
}

void Game::HandlePlayerAction(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& iss, Server& server) {
    std::string action_value;
    if (!(iss >> action_value)) {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Invalid Action\"\n");
        return;
    }

    try {
        int number = std::stoi(action_value);
        Player* current_player = nullptr;

        for (auto& p : _players) {
            if (p.getId() == client_id && p.isConnected()) {
                current_player = &p;
                break;
            }
        }

        if (!current_player) {
            server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Player Not Connected\"\n");
            return;
        }

        if (current_player->hasPlayed()) {
            server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"You have already played in this round\"\n");
            return;
        }

        current_player->setNumber(number);
        current_player->setHasPlayed(true);
        _actions_received++;

        std::cout << "[Game] Player " << client_id << " played: " << number << "\n";
        server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
        server.SendMessage(socket_fd, "LogChannel " + std::to_string(client_id) + " \"You chose number " + std::to_string(number) + "\"\n");

        if (static_cast<size_t>(_actions_received) >= REQUIRED_PLAYERS) {
            std::vector<const Player*> active_players;
            for (const auto& p : _players) {
                if (p.isConnected() && p.hasPlayed()) {
                    active_players.push_back(&p);
                }
            }

            if (active_players.size() >= REQUIRED_PLAYERS) {
                std::string result;
                if (active_players[0]->getNumber() > active_players[1]->getNumber()) {
                    result = "Player " + std::to_string(active_players[0]->getId()) + " won!";
                } else if (active_players[0]->getNumber() < active_players[1]->getNumber()) {
                    result = "Player " + std::to_string(active_players[1]->getId()) + " won!";
                } else {
                    result = "It is a tie!";
                }

                std::cout << "[Game] Match finished: " << result << "\n";
                server.SendMessage(socket_fd, "LogChannel All \"" + result + "\"\n");
                ResetRoundState();
            }
        }
    } catch (...) {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Invalid Action\"\n");
    }
}

void Game::HandleResetRound(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& /*iss*/, Server& server) {
    if (client_id != 0) {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Only the room creator (Client 0) can reset the round\"\n");
        return;
    }

    ResetRoundState();
    std::cout << "[Game] Round administratively reset (Client 0).\n";
    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
    server.SendMessage(socket_fd, "LogChannel All \"Round reset by the room creator!\"\n");
}