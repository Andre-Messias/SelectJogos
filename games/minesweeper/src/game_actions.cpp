#include "game.hpp"
#include "leaderboard.hpp"
#include <iostream>
#include <sstream>

std::string Game::GetRoomPlayersString(int room_id)
{
    std::string out = "[ Jogadores na sala: ";
    bool first = true;
    for (const auto &p : _players)
    {
        if (p.isConnected() && p.getRoomId() == room_id)
        {
            if (!first)
                out += ", ";
            out += "Player " + std::to_string(p.getId());
            first = false;
        }
    }
    out += " ]\n";
    return out;
}

void Game::BroadcastToRoom(int room_id, const std::string &message, Server &server)
{
    std::string safe = message;
    bool is_screen = false;
    
    // Check for our custom screen tag
    if (safe.rfind("@SCREEN_TAG ", 0) == 0) { // starts with
        is_screen = true;
        safe.erase(0, 12); // remove the tag
    }
    
    if (is_screen) {
        // Replace newlines with | for @SCREEN directive
        size_t pos;
        while ((pos = safe.find("\n")) != std::string::npos) {
            safe.replace(pos, 1, " | ");
        }
        
        std::string packet = "LogChannel All \"@SCREEN " + safe + "\"\n";
        for (const auto &p : _players) {
            if (p.isConnected() && p.getRoomId() == room_id) {
                server.SendMessage(p.getSocketFd(), packet);
            }
        }
    } else {
        // Send as regular line-by-line logs
        std::istringstream stream(safe);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty() || line == "\r") continue;
            if (line.back() == '\r') line.pop_back();
            
            std::string packet = "LogChannel All \"" + line + "\"\n";
            for (const auto &p : _players) {
                if (p.isConnected() && p.getRoomId() == room_id) {
                    server.SendMessage(p.getSocketFd(), packet);
                }
            }
        }
    }
}

void Game::HandleJoinRoom(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server)
{
    int room_id;
    if (!(iss >> room_id))
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Sala inválida\"\n");
        return;
    }

    bool room_exists = false;
    Room* target_room = nullptr;
    for (auto &r : _rooms)
    {
        if (room_id == r.GetId())
        {
            room_exists = true;
            target_room = &r;
            break;
        }
    }

    if (!room_exists)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Sala não existe no servidor (1 a 6)\"\n");
        return;
    }

    // Se a Lobby enviou como Admin (0), force todos os jogadores conectados a entrar nessa sala
    if (client_id == 0)
    {
        _active_room_id = room_id; // Set the active difficulty for this server
        for (auto &p : _players)
        {
            p.setRoomId(room_id);
        }
        
        server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
        
        std::string board_render = "@SCREEN_TAG " + GetRoomPlayersString(room_id);
        board_render += "[ STATUS: LOBBY (Digite 'sa StartGame' para começar) ]\n\n";
        board_render += target_room->GetBoard().Render();
        BroadcastToRoom(room_id, board_render, server);
        return;
    }

    // Fluxo normal para client_id específico
    Player *current_player = nullptr;
    int num_players_in_room = 0;

    for (auto &p : _players)
    {
        if (p.getId() == client_id)
        {
            current_player = &p;
            p.updateActivity();
        }

        if (p.getRoomId() == room_id)
        {
            num_players_in_room++;
        }
    }

    if (!current_player)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogador não encontrado\"\n");
        return;
    }

    if (num_players_in_room >= target_room->GetMaxPlayers())
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Sala cheia\"\n");
        return;
    }

    current_player->setRoomId(room_id);
    _active_room_id = room_id;
    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");

    std::string board_render = "@SCREEN_TAG " + GetRoomPlayersString(room_id);
    if (target_room->GetState() == RoomState::LOBBY)
    {
        board_render += "[ STATUS: LOBBY (Digite 'sa StartGame' para começar) ]\n\n";
    }
    else if (target_room->GetState() == RoomState::NAMING)
    {
        board_render += "[ STATUS: AGUARDANDO NOME DA EQUIPE ]\n\n";
    }
    else
    {
        board_render += "[ STATUS: JOGANDO ]\n\n";
    }
    board_render += target_room->GetBoard().Render();
    BroadcastToRoom(room_id, board_render, server);
}

void Game::HandlePlayerAction(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server)
{
    std::string move;

    if (!(iss >> move))
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogada vazia\"\n");
        return;
    }

    for (auto &p : _players)
    {
        if (p.getId() == client_id)
        {
            if (p.getRoomId() == -1)
            {
                server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Entre em uma sala primeiro\"\n");
                return;
            }

            p.updateActivity();

            for (auto &r : _rooms)
            {
                if (r.GetId() == p.getRoomId())
                {
                    if (r.GetState() != RoomState::PLAYING)
                    {
                        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"A partida ainda não começou ou já terminou.\"\n");
                        return;
                    }

                    MoveInput parsed_move;

                    if (!ParseInput(move, r.GetBoard().Size(), parsed_move))
                    {
                        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogada inválida\"\n");
                        return;
                    }

                    if (parsed_move.action == MoveInput::FLAG)
                    {
                        r.GetBoard().SetFlag(parsed_move.row, parsed_move.col, true);
                    }
                    else if (parsed_move.action == MoveInput::UNFLAG)
                    {
                        r.GetBoard().SetFlag(parsed_move.row, parsed_move.col, false);
                    }
                    else
                    {
                        // Gerar o campo na primeira jogada, garantindo área segura
                        if (!r.GetBoard().IsGenerated())
                        {
                            r.GetBoard().Generate(parsed_move.row, parsed_move.col);
                            r.RecordFirstClick();
                        }

                        bool hit_bomb = r.GetBoard().Reveal(parsed_move.row, parsed_move.col);
                        if (hit_bomb)
                        {
                            int penalty = (r.GetId() == 1) ? 15 : ((r.GetId() == 2) ? 20 : 30);
                            r.AddPenalty(penalty);
                            server.SendMessage(socket_fd, "BOOM! Penalidade de tempo adicionada!\n");
                        }
                    }

                    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");

                    // Checa condição de vitória ANTES do render final
                    if (r.GetBoard().IsComplete())
                    {
                        auto now = std::chrono::steady_clock::now();
                        int elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - r.GetStartTime()).count();
                        r.SetWon(elapsed); // Guarda o tempo e atualiza estado
                    }

                    // Limpa a tela e reenvia o tabuleiro pra todos na sala
                    std::string board_render = "@SCREEN_TAG ";
                    board_render += GetRoomPlayersString(r.GetId());
                    if (r.GetState() == RoomState::NAMING)
                    {
                        board_render += "[ STATUS: VITÓRIA! AGUARDANDO NOME DA EQUIPE (!name <nome>) ]\n\n";
                    }
                    else
                    {
                        board_render += "[ STATUS: JOGANDO ]\n\n";
                    }
                    board_render += r.GetBoard().Render();

                    BroadcastToRoom(r.GetId(), board_render, server);

                    if (r.GetState() == RoomState::NAMING)
                    {
                        std::string win_msg = "\nPARABÉNS! Campo limpo em " + std::to_string(r.GetPenaltySeconds()) + "s.\nO jogador que criou a sala deve digitar '!name <nome_da_equipe>' para salvar o recorde!\n";
                        BroadcastToRoom(r.GetId(), win_msg, server);
                    }

                    return;
                }
            }
        }
    }
}

void Game::HandleStartGame(int socket_fd, const std::string &msg_id, int client_id, std::istringstream & /*iss*/, Server &server)
{
    // Se a Lobby enviou como Admin (0), force o início da sala ativa
    if (client_id == 0)
    {
        for (auto &r : _rooms)
        {
            if (r.GetId() == _active_room_id)
            {
                if (r.GetState() != RoomState::LOBBY)
                {
                    server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"A sala já está em andamento.\"\n");
                    return;
                }
                r.Start();
                server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");

                std::string board_render = "@SCREEN_TAG " + GetRoomPlayersString(r.GetId());
                board_render += "[ STATUS: JOGANDO ]\n\n";
                board_render += r.GetBoard().Render();
                BroadcastToRoom(r.GetId(), board_render, server);
                return;
            }
        }
    }

    for (auto &p : _players)
    {
        if (p.getId() == client_id && p.getRoomId() != -1)
        {
            p.updateActivity();
            for (auto &r : _rooms)
            {
                if (r.GetId() == p.getRoomId())
                {
                    if (r.GetState() != RoomState::LOBBY)
                    {
                        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"A sala já está em andamento.\"\n");
                        return;
                    }
                    r.Start();
                    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");

                    std::string board_render = "@SCREEN_TAG ";
                    board_render += GetRoomPlayersString(r.GetId());
                    board_render += "[ STATUS: JOGANDO ]\n\n";
                    board_render += r.GetBoard().Render();
                    BroadcastToRoom(r.GetId(), board_render, server);
                    return;
                }
            }
        }
    }
    server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Você não está em uma sala.\"\n");
}

void Game::HandleNameTeam(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server)
{
    std::string team_name;
    if (!(iss >> team_name))
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Nome da equipe vazio.\"\n");
        return;
    }

    for (auto &p : _players)
    {
        if (p.getId() == client_id && p.getRoomId() != -1)
        {
            p.updateActivity();
            int room_id = p.getRoomId();
            for (auto &r : _rooms)
            {
                if (r.GetId() == room_id)
                {
                    if (r.GetState() != RoomState::NAMING)
                    {
                        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"A sala não está aguardando nome.\"\n");
                        return;
                    }

                    // Check if p is host (first connected player in this room)
                    int host_id = -1;
                    int player_count = 0;
                    for (auto &other : _players)
                    {
                        if (other.isConnected() && other.getRoomId() == room_id)
                        {
                            player_count++;
                            if (host_id == -1)
                                host_id = other.getId();
                        }
                    }

                    if (client_id != host_id)
                    {
                        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Apenas o líder (Player " + std::to_string(host_id) + ") pode nomear a equipe.\"\n");
                        return;
                    }

                    // Save stats
                    std::vector<int> team_ids;
                    for (auto &other : _players)
                    {
                        if (other.isConnected() && other.getRoomId() == room_id)
                        {
                            team_ids.push_back(other.getId());
                        }
                    }
                    Leaderboard::SaveTeamScore(r.GetStatsFile(), team_ids, r.GetPenaltySeconds(), player_count, team_name);

                    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
                    BroadcastToRoom(room_id, "\nRecorde salvo para a equipe '" + team_name + "'!\nVoltando ao LOBBY...\n", server);

                    std::this_thread::sleep_for(std::chrono::seconds(2));

                    r.Reset();

                    std::string board_render = "@SCREEN_TAG ";
                    board_render += GetRoomPlayersString(r.GetId());
                    board_render += "[ STATUS: LOBBY (Digite '!start' para começar) ]\n\n";
                    board_render += r.GetBoard().Render();
                    BroadcastToRoom(r.GetId(), board_render, server);
                    return;
                }
            }
        }
    }
}

void Game::HandleRanking(int socket_fd, const std::string &msg_id, int /*client_id*/, std::istringstream &iss, Server &server)
{
    std::string diff;
    if (!(iss >> diff))
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Especifique a dificuldade (easy, medium, hard).\"\n");
        return;
    }

    std::string file_name;
    if (diff == "easy")
        file_name = "stats_easy.txt";
    else if (diff == "medium")
        file_name = "stats_medium.txt";
    else if (diff == "hard")
        file_name = "stats_hard.txt";
    else
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Dificuldade inválida.\"\n");
        return;
    }

    std::string ranking_str = Leaderboard::GetRankingString(diff, file_name);

    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
    server.SendMessage(socket_fd, ranking_str + "\n");
}
