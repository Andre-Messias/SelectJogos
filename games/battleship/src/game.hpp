#pragma once
#include "board.hpp"
#include "server.hpp"
#include <array>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

class Game {
public:
    void ProcessMessage(int socket_fd, const std::string& line, Server& server);
private:
    enum class Phase { Waiting, Placement, Battle, Finished };
    struct Player {
        int id;
        std::string name;
        bool connected = true;
        bool ready = false;
        bool own_view = true;
        int page = 1;
        int score = 0;
        std::array<int, 4> placed{{0, 0, 0, 0}};
        Board board;
        Player(int client_id, std::string player_name, int size)
            : id(client_id), name(std::move(player_name)), board(size) {}
    };

    std::mutex mutex_;
    std::vector<Player> players_;
    std::unordered_set<int> spectators_;
    int mode_ = 0;
    Phase phase_ = Phase::Waiting;
    int turn_ = 0;

    Player* Find(int id);
    void Reply(Server& server, int fd, const std::string& mid, bool ok, const std::string& reason = "");
    void Log(Server& server, const std::string& target, const std::string& message);
    void SendScreen(Server& server, Player& player);
    void RefreshScreens(Server& server);
    void Connect(Server& server, int fd, const std::string& mid, int id, std::istream& args, bool reconnect);
    void Disconnect(Server& server, int fd, const std::string& mid, int id);
    void SetName(Server& server, int fd, const std::string& mid, int id, std::istream& args);
    void SetMode(Server& server, int fd, const std::string& mid, int id, std::istream& args);
    void PlaceShip(Server& server, int fd, const std::string& mid, int id, std::istream& args);
    void Ready(Server& server, int fd, const std::string& mid, int id);
    void Fire(Server& server, int fd, const std::string& mid, int id, std::istream& args);
    void View(Server& server, int fd, const std::string& mid, int id, std::istream& args);
};
