#pragma once

#include "player.hpp"
#include "server.hpp"

#include <istream>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

// Mantém o estado da partida e responde aos comandos enviados pelo lobby.
class Game {
public:
    // Recebe o protocolo: <Comando> <MsgID> <ClientID> [argumentos].
    void ProcessMessage(int socket_fd, const std::string& line, Server& server);

private:
    // O início da batalha depende da confirmação das duas frotas.
    enum class Phase {
        Waiting,
        Placement,
        Battle,
        Finished
    };

    std::mutex mutex_;
    std::vector<Player> players_;
    // Jogadores excedentes não recebem tabuleiros privados.
    std::unordered_set<int> spectators_;
    int mode_ = 0;
    Phase phase_ = Phase::Waiting;
    int turn_ = 0;

    // Busca um dos jogadores pelo identificador recebido do lobby.
    Player* Find(int id);

    // A resposta preserva o MsgID para o lobby entregá-la ao jogador correto.
    void Reply(Server& server, int fd, const std::string& mid, bool ok, const std::string& reason = "");

    // Envia um aviso a todos ou a um jogador específico.
    void Log(Server& server, const std::string& target, const std::string& message);

    // Envia uma tela individual para proteger as posições da frota inimiga.
    void SendScreen(Server& server, Player& player);

    // Atualiza a tela de cada jogador conectado.
    void RefreshScreens(Server& server);

    // Trata os avisos de entrada, saída e retorno enviados pelo lobby.
    void Connect(Server& server, int fd, const std::string& mid, int id, std::istream& args, bool reconnect);

    void Disconnect(Server& server, int fd, const std::string& mid, int id);

    void SetName(Server& server, int fd, const std::string& mid, int id, std::istream& args);

    // Apenas ServerAction do criador chega com ClientID efetivo igual a zero.
    void SetMode(Server& server, int fd, const std::string& mid, int id, std::istream& args);

    // Comandos da partida: montar frota, confirmar, atacar e consultar vista.
    void PlaceShip(Server& server, int fd, const std::string& mid, int id, std::istream& args);

    void Ready(Server& server, int fd, const std::string& mid, int id);

    void Fire(Server& server, int fd, const std::string& mid, int id, std::istream& args);

    void View(Server& server, int fd, const std::string& mid, int id, std::istream& args);
};
