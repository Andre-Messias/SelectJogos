#include "game.hpp"
#include <iostream>
#include <sstream>

void Game::InactivityCheckerLoop(Server &server)
{
    while (_is_running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10)); // Checa a cada 10s

        std::lock_guard<std::mutex> lock(_game_mutex);
        for (auto &p : _players)
        {
            if (p.isConnected() && p.isInactive(300)) // 5 minutos = 300 segundos
            {
                std::cout << "[Game] Player " << p.getId() << " kickado por inatividade.\n";
                server.SendMessage(p.getSocketFd(), "Você foi desconectado por inatividade (> 5 min).\n");

                int p_room_id = p.getRoomId();

                p.setRoomId(-1);
                p.setConnected(false);

                if (p_room_id != -1)
                {
                    // Checar se a sala ficou vazia
                    bool is_empty = true;
                    for (auto &other : _players)
                    {
                        if (other.isConnected() && other.getRoomId() == p_room_id)
                        {
                            is_empty = false;
                            break;
                        }
                    }

                    for (auto &r : _rooms)
                    {
                        if (r.GetId() == p_room_id)
                        {
                            if (is_empty)
                            {
                                r.Reset(); // Sala vazia, reseta
                            }
                            else
                            {
                                // Atualiza a tela de quem ficou
                                std::string board_render = "@SCREEN_TAG ";
                                board_render += GetRoomPlayersString(r.GetId());
                                if (r.GetState() == RoomState::NAMING)
                                {
                                    board_render += "[ STATUS: VITÓRIA! AGUARDANDO NOME DA EQUIPE (!name <nome>) ]\n\n";
                                }
                                else if (r.GetState() == RoomState::LOBBY)
                                {
                                    board_render += "[ STATUS: LOBBY (Digite '!start' para começar) ]\n\n";
                                }
                                else
                                {
                                    board_render += "[ STATUS: JOGANDO ]\n\n";
                                }
                                board_render += r.GetBoard().Render();
                                BroadcastToRoom(r.GetId(), board_render, server);
                            }
                        }
                    }
                }
            }
        }
    }
}

void Game::HandleConnectClient(int socket_fd, const std::string &msg_id, int client_id, std::istringstream & /*iss*/, Server &server)
{
    for (auto &p : _players)
    {
        if (p.getId() == client_id)
        {
            if (p.isConnected())
            {
                server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogador já está online\"\n");
                return;
            }
            p.setConnected(true);
            p.setSocketFd(socket_fd);
            p.updateActivity();
            std::cout << "[Game] Player " << client_id << " reconectado.\n";
            server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
            server.SendMessage(socket_fd, "LogChannel All \"Player " + std::to_string(client_id) + " voltou.\"\n");
            
            // Re-render board for reconnection
            for (auto &r : _rooms) {
                if (r.GetId() == _active_room_id) {
                    std::string board_render = "@SCREEN_TAG " + GetRoomPlayersString(r.GetId());
                    if (r.GetState() == RoomState::NAMING) board_render += "[ STATUS: VITÓRIA! AGUARDANDO NOME (!name <nome>) ]\n\n";
                    else if (r.GetState() == RoomState::LOBBY) board_render += "[ STATUS: LOBBY (Digite 'sa StartGame' para começar) ]\n\n";
                    else board_render += "[ STATUS: JOGANDO ]\n\n";
                    board_render += r.GetBoard().Render();
                    BroadcastToRoom(r.GetId(), board_render, server);
                    break;
                }
            }
            return;
        }
    }

    _players.emplace_back(client_id, socket_fd);
    _players.back().setRoomId(_active_room_id); // Auto-assign to the currently active difficulty
    
    std::cout << "[Game] Player " << client_id << " entrou.\n";
    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
    server.SendMessage(socket_fd, "LogChannel All \"Player " + std::to_string(client_id) + " entrou no servidor.\"\n");
    
    // Broadcast updated board to everyone so they see the new player
    for (auto &r : _rooms) {
        if (r.GetId() == _active_room_id) {
            std::string board_render = "@SCREEN_TAG " + GetRoomPlayersString(r.GetId());
            if (r.GetState() == RoomState::NAMING) board_render += "[ STATUS: VITÓRIA! AGUARDANDO NOME (!name <nome>) ]\n\n";
            else if (r.GetState() == RoomState::LOBBY) board_render += "[ STATUS: LOBBY (Digite 'sa StartGame' para começar) ]\n\n";
            else board_render += "[ STATUS: JOGANDO ]\n\n";
            board_render += r.GetBoard().Render();
            BroadcastToRoom(r.GetId(), board_render, server);
            break;
        }
    }
}

void Game::HandleDisconnectClient(int socket_fd, const std::string &msg_id, int client_id, std::istringstream & /*iss*/, Server &server)
{
    for (auto &p : _players)
    {
        if (p.getId() == client_id)
        {
            int p_room_id = p.getRoomId();

            if (p_room_id != -1)
            {
                p.setRoomId(-1);

                // Checar se a sala ficou vazia
                bool is_empty = true;
                for (auto &other : _players)
                {
                    if (other.isConnected() && other.getRoomId() == p_room_id)
                    {
                        is_empty = false;
                        break;
                    }
                }

                if (is_empty)
                {
                    for (auto &r : _rooms)
                    {
                        if (r.GetId() == p_room_id)
                        {
                            r.Reset();
                        }
                    }
                }
                else
                {
                    for (auto &r : _rooms)
                    {
                        if (r.GetId() == p_room_id)
                        {
                            std::string board_render = "@SCREEN_TAG ";
                            board_render += GetRoomPlayersString(r.GetId());
                            if (r.GetState() == RoomState::NAMING)
                            {
                                board_render += "[ STATUS: VITÓRIA! AGUARDANDO NOME DA EQUIPE (!name <nome>) ]\n\n";
                            }
                            else if (r.GetState() == RoomState::LOBBY)
                            {
                                board_render += "[ STATUS: LOBBY (Digite '!start' para começar) ]\n\n";
                            }
                            else
                            {
                                board_render += "[ STATUS: JOGANDO ]\n\n";
                            }
                            board_render += r.GetBoard().Render();
                            BroadcastToRoom(r.GetId(), board_render, server);
                        }
                    }
                }
            }

            p.setConnected(false);
            server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
            std::cout << "[Game] Player " << client_id << " desconectado.\n";
            return;
        }
    }
    server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogador não encontrado\"\n");
}

void Game::HandleReconnectClient(int socket_fd, const std::string &msg_id, int client_id, std::istringstream & /*iss*/, Server &server)
{
    for (auto &p : _players)
    {
        if (p.getId() == client_id)
        {
            p.setConnected(true);
            p.setSocketFd(socket_fd);
            p.updateActivity();
            std::cout << "[Game] Player " << client_id << " reconectado.\n";
            server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
            server.SendMessage(socket_fd, "LogChannel All \"Player " + std::to_string(client_id) + " voltou.\"\n");
            return;
        }
    }

    _players.emplace_back(client_id, socket_fd);
    
    std::cout << "[Game] Player " << client_id << " reconectado (novo).\n";
    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
}
