#include "game.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>

namespace
{
    std::string Lower(std::string value)
    {
        for (char &c : value)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return value;
    }

    bool NoMore(std::istream &args)
    {
        std::string extra;
        return !(args >> extra);
    }

    std::string DisplayName(int id, const std::string &nick)
    {
        return nick.empty() ? "Player " + std::to_string(id) : nick;
    }
} // namespace anônimo

Player *Game::Find(int id)
{
    for (auto &player : players_)
    {
        if (player.id == id)
        {
            return &player;
        }
    }
    return nullptr;
}

void Game::Reply(Server &server, int fd, const std::string &mid, bool ok, const std::string &reason)
{
    std::string line = "Response " + mid + (ok ? " Success" : " Fail");
    if (!reason.empty())
    {
        line += " \"" + reason + "\"";
    }
    server.SendMessage(fd, line + "\n");
}

void Game::Log(Server &server, const std::string &target, const std::string &message)
{
    server.Broadcast("LogChannel " + target + " \"" + message + "\"\n");
}

void Game::SendScreen(Server &server, Player &player)
{
    if (!player.connected)
    {
        return;
    }

    const FleetMode &mode = MODES[mode_];
    std::vector<std::string> lines;
    lines.push_back("BATALHA NAVAL " + std::string(mode.name) + " - " + player.name);

    if (phase_ == Phase::Waiting)
    {
        lines.push_back("Aguardando o segundo jogador.");
    }
    else if (phase_ == Phase::Placement)
    {
        lines.push_back(player.ready ? "Frota pronta. Aguardando adversario."
                                     : "Posicione a frota e digite Ready.");
    }
    else if (phase_ == Phase::Battle)
    {
        lines.push_back(players_[turn_].id == player.id ? "SUA VEZ: Fire <Coordenada>"
                                                        : "Aguarde o adversario.");
    }
    else
    {
        lines.push_back("Partida encerrada. Use st e sg para iniciar outra.");
    }

    // Cada jogador recebe sua própria vista; navios inimigos ficam ocultos.
    bool own = phase_ != Phase::Battle || player.own_view;
    const Board *board = &player.board;
    if (!own && players_.size() == 2)
    {
        board = &players_[players_[0].id == player.id ? 1 : 0].board;
    }

    int max_page = (mode.size + 9) / 10;
    int page = player.VisiblePage(max_page);
    lines.push_back(std::string(own ? "Minha frota" : "Alvo") + " - pagina " +
                    std::to_string(page) + "/" + std::to_string(max_page));

    auto board_lines = board->Render(own, page);
    lines.insert(lines.end(), board_lines.begin(), board_lines.end());

    if (phase_ == Phase::Placement && !player.ready)
    {
        lines.push_back(player.RemainingShips(mode));
    }
    lines.push_back("View own|enemy [pagina] / Fire A1");

    // O protocolo separa as linhas da tela com '|'.
    std::string payload = "@SCREEN ";
    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (i > 0)
        {
            payload += "|";
        }
        payload += lines[i];
    }
    Log(server, std::to_string(player.id), payload);
}

void Game::RefreshScreens(Server &server)
{
    for (auto &player : players_)
    {
        SendScreen(server, player);
    }
}

// O lobby registra os dois participantes e avisa quando algum deles retorna.
void Game::Connect(Server &server, int fd, const std::string &mid, int id, std::istream &args, bool reconnect)
{
    int room_id = 0;
    std::string nick;
    if (!reconnect)
    {
        args >> room_id >> nick;
    }

    Player *player = Find(id);
    if (player)
    {
        player->connected = true;
        if (!nick.empty())
        {
            player->name = DisplayName(id, nick);
        }
    }
    else if (players_.size() < 2 && phase_ == Phase::Waiting)
    {
        players_.emplace_back(id, DisplayName(id, nick), MODES[mode_].size);
        player = &players_.back();
    }
    else
    {
        // O lobby não impõe limite de duas pessoas à sala.
        spectators_.insert(id);
        Reply(server, fd, mid, true);
        Log(server, std::to_string(id),
            "Partida cheia: voce esta como espectador. Use lv para sair.");
        return;
    }

    if (players_.size() == 2 && phase_ == Phase::Waiting)
    {
        phase_ = Phase::Placement;
    }
    Reply(server, fd, mid, true);
    RefreshScreens(server);
    if (phase_ == Phase::Battle)
    {
        Log(server, "All", player->name + " voltou a partida.");
    }
}

void Game::Disconnect(Server &server, int fd, const std::string &mid, int id)
{
    Player *player = Find(id);
    if (player)
    {
        player->connected = false;
    }
    spectators_.erase(id);
    Reply(server, fd, mid, true);

    if (player)
    {
        Log(server, "All", player->name + " saiu. A partida aguarda sua reconexao.");
        RefreshScreens(server);
    }
}

void Game::SetName(Server &server, int fd, const std::string &mid, int id, std::istream &args)
{
    Player *player = Find(id);
    std::string nick;
    if (!player || !(args >> nick) || !NoMore(args))
    {
        Reply(server, fd, mid, false, "Nome invalido");
        return;
    }

    player->name = nick;
    Reply(server, fd, mid, true);
    RefreshScreens(server);
}

// A troca de modo só é segura antes de posicionar o primeiro navio.
void Game::SetMode(Server &server, int fd, const std::string &mid, int id, std::istream &args)
{
    if (id != 0)
    {
        Reply(server, fd, mid, false, "Apenas o criador pode escolher o modo");
        return;
    }

    std::string name;
    if (!(args >> name) || !NoMore(args))
    {
        Reply(server, fd, mid, false, "Use sa SetMode rapido|classico|longo");
        return;
    }
    name = Lower(name);

    int chosen = -1;
    for (int i = 0; i < 3; ++i)
    {
        if (name == MODES[i].name)
        {
            chosen = i;
        }
    }
    if (chosen < 0)
    {
        Reply(server, fd, mid, false, "Modo invalido");
        return;
    }
    if (phase_ == Phase::Battle || phase_ == Phase::Finished)
    {
        Reply(server, fd, mid, false, "Partida ja iniciada");
        return;
    }

    for (const auto &player : players_)
    {
        if (player.HasPlacedShips())
        {
            Reply(server, fd, mid, false,
                  "Nao e possivel trocar o modo apos posicionar navios");
            return;
        }
    }

    mode_ = chosen;
    for (auto &player : players_)
    {
        player.ResetBoard(MODES[mode_].size);
    }
    Reply(server, fd, mid, true);
    Log(server, "All", "Modo: " + name);
    RefreshScreens(server);
}

// Valida tipo, quantidade e posição antes de confirmar a jogada.
void Game::PlaceShip(Server &server, int fd, const std::string &mid, int id, std::istream &args)
{
    Player *player = Find(id);
    if (!player || !player->connected || phase_ != Phase::Placement ||
        player->ready)
    {
        Reply(server, fd, mid, false, "Posicionamento indisponivel");
        return;
    }

    std::string type, coord, direction;
    if (!(args >> type >> coord >> direction) || !NoMore(args))
    {
        Reply(server, fd, mid, false,
              "Use PlaceShip submarino|cruzador|encouracado|portaaviao A1 H|V");
        return;
    }
    type = Lower(type);
    direction = Lower(direction);

    int ship = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (type == SHIP_NAMES[i])
        {
            ship = i;
        }
    }
    if (!player->CanPlaceShip(ship, MODES[mode_]) ||
        (direction != "h" && direction != "v"))
    {
        Reply(server, fd, mid, false, "Tipo, quantidade ou direcao invalida");
        return;
    }

    int x, y;
    if (!ParseCoordinate(coord, MODES[mode_].size, x, y) ||
        !player->PlaceShip(ship, x, y, direction == "v"))
    {
        Reply(server, fd, mid, false,
              "Posicao invalida: navio fora do tabuleiro ou colado a outro");
        return;
    }

    Reply(server, fd, mid, true);
    SendScreen(server, *player);
}

// Os ataques começam somente após a confirmação das duas frotas.
void Game::Ready(Server &server, int fd, const std::string &mid, int id)
{
    Player *player = Find(id);
    if (!player || !player->connected || phase_ != Phase::Placement ||
        player->ready)
    {
        Reply(server, fd, mid, false, "Ready indisponivel");
        return;
    }
    if (!player->HasCompleteFleet(MODES[mode_]))
    {
        Reply(server, fd, mid, false,
              "Posicione todos os navios antes de Ready");
        return;
    }

    player->ready = true;
    Reply(server, fd, mid, true);
    Log(server, "All", player->name + " terminou de posicionar a frota.");

    if (players_.size() == 2 && players_[0].ready && players_[1].ready)
    {
        phase_ = Phase::Battle;
        turn_ = 0;
        for (auto &member : players_)
        {
            member.BeginBattleView();
        }
        Log(server, "All", "Batalha iniciada. " + players_[turn_].name + " comeca.");
    }
    RefreshScreens(server);
}

// Um tiro válido passa a vez ao adversário, mesmo quando acerta.
void Game::Fire(Server &server, int fd, const std::string &mid, int id, std::istream &args)
{
    Player *player = Find(id);
    if (!player || !player->connected || phase_ != Phase::Battle ||
        players_.size() != 2)
    {
        Reply(server, fd, mid, false, "Batalha indisponivel");
        return;
    }
    if (!players_[0].connected || !players_[1].connected)
    {
        Reply(server, fd, mid, false, "Aguarde o adversario reconectar");
        return;
    }
    if (players_[turn_].id != id)
    {
        Reply(server, fd, mid, false, "Nao e sua vez");
        return;
    }

    std::string coord;
    int x, y;
    if (!(args >> coord) || !NoMore(args) ||
        !ParseCoordinate(coord, MODES[mode_].size, x, y))
    {
        Reply(server, fd, mid, false, "Use Fire A1");
        return;
    }

    Player &target = players_[1 - turn_];
    Shot shot = target.board.Fire(x, y);
    if (shot == Shot::Invalid)
    {
        Reply(server, fd, mid, false, "Posicao ja atacada");
        return;
    }

    Reply(server, fd, mid, true);
    std::string result = shot == Shot::Miss ? "agua" : shot == Shot::Sunk ? "afundou um navio"
                                                                          : "acertou um navio";
    Log(server, "All", player->name + " atacou " + coord + ": " + result + ".");

    // Vence quem atingir todas as casas da frota adversária.
    if (target.board.AllSunk())
    {
        phase_ = Phase::Finished;
        Log(server, "All", player->name + " venceu! Afundou todos os navios de " + target.name + ".");
    }
    else
    {
        turn_ = 1 - turn_;
    }
    RefreshScreens(server);
}

// A escolha da vista é individual e não altera o estado do adversário.
void Game::View(Server &server, int fd, const std::string &mid, int id, std::istream &args)
{
    Player *player = Find(id);
    std::string side, page_text;
    if (!player || !player->connected || !(args >> side))
    {
        Reply(server, fd, mid, false, "Use View own|enemy [pagina]");
        return;
    }
    side = Lower(side);
    if (side != "own" && side != "enemy")
    {
        Reply(server, fd, mid, false, "Use View own|enemy [pagina]");
        return;
    }

    int page = player->page;
    if (args >> page_text)
    {
        if (!NoMore(args))
        {
            Reply(server, fd, mid, false, "Pagina invalida");
            return;
        }
        try
        {
            size_t used = 0;
            page = std::stoi(page_text, &used);
            if (used != page_text.size())
            {
                throw std::invalid_argument("page");
            }
        }
        catch (...)
        {
            Reply(server, fd, mid, false, "Pagina invalida");
            return;
        }
    }
    if (page < 1 || page > (MODES[mode_].size + 9) / 10)
    {
        Reply(server, fd, mid, false, "Pagina invalida");
        return;
    }

    player->SetView(side == "own", page);
    Reply(server, fd, mid, true);
    SendScreen(server, *player);
}

void Game::ProcessMessage(int fd, const std::string &line, Server &server)
{
    std::istringstream args(line);
    std::string command, mid;
    int id;
    if (!(args >> command >> mid >> id))
    {
        return;
    }

    // Protege o estado da partida acessado pelas threads TCP.
    std::lock_guard<std::mutex> lock(mutex_);
    if (command == "ConnectClient")
    {
        Connect(server, fd, mid, id, args, false);
    }
    else if (command == "ReconnectClient")
    {
        Connect(server, fd, mid, id, args, true);
    }
    else if (command == "DisconnectClient")
    {
        Disconnect(server, fd, mid, id);
    }
    else if (command == "SetPlayerName")
    {
        SetName(server, fd, mid, id, args);
    }
    else if (command == "SetMode")
    {
        SetMode(server, fd, mid, id, args);
    }
    else if (command == "PlaceShip")
    {
        PlaceShip(server, fd, mid, id, args);
    }
    else if (command == "Ready")
    {
        Ready(server, fd, mid, id);
    }
    else if (command == "Fire")
    {
        Fire(server, fd, mid, id, args);
    }
    else if (command == "View")
    {
        View(server, fd, mid, id, args);
    }
    else
    {
        Reply(server, fd, mid, false, "Comando desconhecido");
    }
}
