#include "game.hpp"
#include "leaderboard.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

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
            out += p.getName();
            first = false;
        }
    }
    out += " ]\n";
    return out;
}

Player *Game::FindPlayer(int client_id)
{
    for (auto &p : _players)
    {
        if (p.getId() == client_id)
            return &p;
    }
    return nullptr;
}

Room *Game::FindRoom(int room_id)
{
    for (auto &r : _rooms)
    {
        if (r.GetId() == room_id)
            return &r;
    }
    return nullptr;
}

void Game::BroadcastRoomScreen(Room &room, Server &server)
{
    std::string screen = "@SCREEN_TAG " + GetRoomPlayersString(room.GetId());
    switch (room.GetState())
    {
    case RoomState::LOBBY:
        screen += "[ STATUS: LOBBY (Digite '!start' para começar) ]\n\n";
        break;
    case RoomState::NAMING:
        screen += "[ STATUS: VITÓRIA! AGUARDANDO NOME DA EQUIPE (!name <nome>) ]\n\n";
        break;
    default:
        screen += "[ STATUS: JOGANDO ]\n\n";
        break;
    }
    screen += room.GetBoard().Render();
    BroadcastToRoom(room.GetId(), screen, server);
}

void Game::BroadcastToRoom(int /*room_id*/, const std::string &message, Server &server)
{
    std::string safe = message;
    bool is_screen = false;

    // Check for our custom screen tag
    if (safe.rfind("@SCREEN_TAG ", 0) == 0)
    { // starts with
        is_screen = true;
        safe.erase(0, 12); // remove the tag
    }

    if (is_screen)
    {
        // Replace newlines with | for @SCREEN directive
        size_t pos;
        while ((pos = safe.find("\n")) != std::string::npos)
        {
            safe.replace(pos, 1, "|");
        }

        std::string packet = "LogChannel All \"@SCREEN " + safe + "\"\n";
        server.Broadcast(packet);
    }
    else
    {
        // Send as regular line-by-line logs
        std::istringstream stream(safe);
        std::string line;
        while (std::getline(stream, line))
        {
            if (line.empty() || line == "\r")
                continue;
            if (line.back() == '\r')
                line.pop_back();

            std::string packet = "LogChannel All \"" + line + "\"\n";
            server.Broadcast(packet);
        }
    }
}

void Game::HandleJoinRoom(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server)
{
    int room_id;
    if (!(iss >> room_id) || room_id < 1 || room_id > 3)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Sala inválida (use 1 a 3)\"\n");
        return;
    }

    if (client_id == 0)
    {
        _active_room_id = room_id;
        server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
        Room *r = FindRoom(room_id);
        if (r) {
            std::string board_render = "@SCREEN_TAG ";
            board_render += GetRoomPlayersString(r->GetId());
            if (r->GetState() == RoomState::NAMING)
                board_render += "[ STATUS: VITÓRIA! AGUARDANDO NOME DA EQUIPE (!name <nome>) ]\n\n";
            else
                board_render += "[ STATUS: LOBBY (Digite '!start' para começar) ]\n\n";
            board_render += r->GetBoard().Render();
            BroadcastToRoom(room_id, board_render, server);
        }
        return;
    }

    Player *current_player = FindPlayer(client_id);
    if (!current_player)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogador não encontrado\"\n");
        return;
    }

    if (current_player->getRoomId() == room_id)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Você já está nesta sala\"\n");
        return;
    }

    Room *r = FindRoom(room_id);
    if (!r)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Sala não encontrada\"\n");
        return;
    }

    if (r->GetState() != RoomState::LOBBY)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogo em andamento\"\n");
        return;
    }

    int current_players = 0;
    for (const auto &p : _players)
    {
        if (p.getRoomId() == room_id && p.isConnected())
            current_players++;
    }

    if (current_players >= r->GetMaxPlayers())
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Sala cheia\"\n");
        return;
    }

    current_player->setRoomId(room_id);
    current_player->updateActivity();

    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
    BroadcastToRoom(room_id, "O jogador " + current_player->getName() + " entrou na sala!\n", server);
    BroadcastRoomScreen(*r, server);
}

void Game::HandlePlayerAction(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server)
{
    std::string move;

    if (!(iss >> move))
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogada vazia\"\n");
        return;
    }

    Player *p = FindPlayer(client_id);
    if (!p)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogador não encontrado\"\n");
        return;
    }

    if (p->getRoomId() == -1)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Entre em uma sala primeiro\"\n");
        return;
    }

    p->updateActivity();

    Room *r = FindRoom(p->getRoomId());
    if (!r)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Sala não encontrada\"\n");
        return;
    }

    if (r->GetState() != RoomState::PLAYING)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"A partida ainda não começou ou já terminou.\"\n");
        return;
    }

    MoveInput parsed_move;
    if (!ParseInput(move, r->GetBoard().Size(), parsed_move))
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogada inválida\"\n");
        return;
    }

    if (parsed_move.action == MoveInput::FLAG)
    {
        r->GetBoard().SetFlag(parsed_move.row, parsed_move.col, true);
    }
    else if (parsed_move.action == MoveInput::UNFLAG)
    {
        r->GetBoard().SetFlag(parsed_move.row, parsed_move.col, false);
    }
    else
    {
        if (!r->GetBoard().IsGenerated())
        {
            r->GetBoard().Generate(parsed_move.row, parsed_move.col);
            r->RecordFirstClick();
        }

        bool hit_bomb = r->GetBoard().Reveal(parsed_move.row, parsed_move.col);
        if (hit_bomb)
        {
            int penalty = (r->GetId() == 1) ? 15 : ((r->GetId() == 2) ? 20 : 30);
            r->AddPenalty(penalty);
            server.SendMessage(socket_fd, "BOOM! Penalidade de tempo adicionada!\n");
        }
    }

    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");

    if (r->GetBoard().IsComplete())
    {
        auto now = std::chrono::steady_clock::now();
        int elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - r->GetStartTime()).count();
        r->SetWon(elapsed);
    }

    BroadcastRoomScreen(*r, server);

    if (r->GetState() == RoomState::NAMING)
    {
        std::string win_msg = "\nPARABÉNS! Campo limpo em " + std::to_string(r->GetPenaltySeconds()) + "s.\nO jogador que criou a sala deve digitar '!name <nome_da_equipe>' para salvar o recorde!\n";
        BroadcastToRoom(r->GetId(), win_msg, server);
    }
}

void Game::HandleStartGame(int socket_fd, const std::string &msg_id, int client_id, std::istringstream & /*iss*/, Server &server)
{
    if (client_id == 0)
    {
        Room *r = FindRoom(_active_room_id);
        if (r) {
            r->Start();
            server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
            BroadcastToRoom(r->GetId(), "A partida começou! Boa sorte!\n", server);
            BroadcastRoomScreen(*r, server);
        }
        return;
    }

    Player *p = FindPlayer(client_id);
    if (!p || p->getRoomId() == -1)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Entre em uma sala primeiro\"\n");
        return;
    }

    Room *r = FindRoom(p->getRoomId());
    if (!r) return;

    if (r->GetState() != RoomState::LOBBY)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"A partida já começou\"\n");
        return;
    }

    r->Start();
    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
    BroadcastToRoom(r->GetId(), "A partida começou! Boa sorte!\n", server);
    BroadcastRoomScreen(*r, server);
}

void Game::HandleNameTeam(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server)
{
    std::string team_name;
    std::getline(iss >> std::ws, team_name);
    if (team_name.empty())
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Nome da equipe não pode ser vazio\"\n");
        return;
    }
    std::replace(team_name.begin(), team_name.end(), ' ', '_');

    Room *r = nullptr;

    if (client_id == 0)
    {
        r = FindRoom(_active_room_id);
    }
    else
    {
        Player *p = FindPlayer(client_id);
        if (!p)
        {
            server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Jogador não encontrado\"\n");
            return;
        }

        if (p->getRoomId() == -1)
        {
            server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"Entre em uma sala primeiro\"\n");
            return;
        }
        r = FindRoom(p->getRoomId());
    }

    if (!r) return;

    if (r->GetState() != RoomState::NAMING)
    {
        server.SendMessage(socket_fd, "Response " + msg_id + " Fail \"O jogo não está aguardando um nome\"\n");
        return;
    }

    server.SendMessage(socket_fd, "Response " + msg_id + " Success\n");
    
    std::vector<std::string> team_members;
    for (const auto &p_check : _players) {
        if (p_check.getRoomId() == r->GetId() && p_check.isConnected()) {
            team_members.push_back(p_check.getName());
        }
    }

    Leaderboard::SaveTeamScore(r->GetStatsFile(), team_members, r->GetPenaltySeconds(), team_members.size(), team_name);
    
    BroadcastToRoom(r->GetId(), "\nRecorde salvo para a equipe '" + team_name + "'!\nVoltando ao LOBBY...\n", server);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    r->Reset();
    BroadcastRoomScreen(*r, server);
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
