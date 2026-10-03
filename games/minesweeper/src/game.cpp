#include "game.hpp"
#include <iostream>

Game::Game() : _is_running(false)
{
    InitRooms();
    RegisterCommands();
}

Game::~Game()
{
    _is_running = false;
    if (_inactivity_thread.joinable()) {
        _inactivity_thread.join();
    }
}

void Game::InitRooms()
{
    _rooms.emplace_back(1, 10, 10, 4, "stats_easy.txt");   // 1 = Easy
    _rooms.emplace_back(2, 16, 40, 4, "stats_medium.txt"); // 2 = Medium
    _rooms.emplace_back(3, 24, 99, 4, "stats_hard.txt");   // 3 = Hard
}

void Game::Start(Server& server)
{
    _is_running = true;
    _inactivity_thread = std::thread(&Game::InactivityCheckerLoop, this, std::ref(server));
}

void Game::ProcessMessage(int socket_fd, const std::string &message, Server &server)
{
    std::istringstream iss(message);
    std::string command, msg_id, client_id_str;

    if (!(iss >> command >> msg_id >> client_id_str))
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Formato de mensagem inválido\"\n");
        return;
    }

    int client_id;
    try
    {
        client_id = std::stoi(client_id_str);
    }
    catch (const std::exception &)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Client ID inválido\"\n");
        return;
    }

    std::lock_guard<std::mutex> lock(_game_mutex);

    auto it = _command_registry.find(command);
    if (it != _command_registry.end())
    {
        it->second(socket_fd, msg_id, client_id, iss, server);
    }
    else
    {
        // Silently ignore unknown Lobby commands so they don't spam the UI
        server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
    }
}

void Game::RegisterCommands()
{
    _command_registry["ConnectClient"] = [this](int fd, const std::string &mid, int cid, std::istringstream &iss, Server &srv)
    {
        this->HandleConnectClient(fd, mid, cid, iss, srv);
    };

    _command_registry["DisconnectClient"] = [this](int fd, const std::string &mid, int cid, std::istringstream &iss, Server &srv)
    {
        this->HandleDisconnectClient(fd, mid, cid, iss, srv);
    };

    _command_registry["ReconnectClient"] = [this](int fd, const std::string &mid, int cid, std::istringstream &iss, Server &srv)
    {
        this->HandleReconnectClient(fd, mid, cid, iss, srv);
    };

    _command_registry["JoinRoom"] = [this](int fd, const std::string &mid, int cid, std::istringstream &iss, Server &srv)
    {
        this->HandleJoinRoom(fd, mid, cid, iss, srv);
    };

    _command_registry["PlayerAction"] = [this](int fd, const std::string &mid, int cid, std::istringstream &iss, Server &srv)
    {
        this->HandlePlayerAction(fd, mid, cid, iss, srv);
    };
    
    _command_registry["StartGame"] = [this](int fd, const std::string &mid, int cid, std::istringstream &iss, Server &srv)
    {
        this->HandleStartGame(fd, mid, cid, iss, srv);
    };
    
    _command_registry["NameTeam"] = [this](int fd, const std::string &mid, int cid, std::istringstream &iss, Server &srv)
    {
        this->HandleNameTeam(fd, mid, cid, iss, srv);
    };
    
    _command_registry["Ranking"] = [this](int fd, const std::string &mid, int cid, std::istringstream &iss, Server &srv)
    {
        this->HandleRanking(fd, mid, cid, iss, srv);
    };
}
