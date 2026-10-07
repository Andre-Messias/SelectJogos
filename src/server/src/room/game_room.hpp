#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <atomic>

#include "game_config.hpp"
#include "game_bridge.hpp"

struct PendingRequest
{
    int sender_id;
    std::string original_msg_id;
};

class GameRoom : public std::enable_shared_from_this<GameRoom>
{
public:
    GameRoom(int id, const std::string &name, const std::string &password, int creator_id, const GameConfig &game_config);

    ~GameRoom();

    int GetId() const;

    std::string GetName() const;

    std::string GetGameName() const;

    bool HasPassword() const;

    bool CheckPassword(const std::string &pass) const;

    bool IsPlaying() const;

    bool IsCreator(int client_id) const;

    int GetCreatorId() const;

    size_t GetPlayerCount() const;

    bool HasClient(int client_id);

    void AddClient(int client_id, int socket_fd, const std::string &nickname = "");

    bool RemoveClient(int client_id, bool permanent = false);

    void SetClientName(int client_id, const std::string &name);

    bool StartGame(int allocated_port);

    void ForwardCommandToGame(int sender_id, int effective_id, const std::string &command, const std::string &msg_id, const std::string &params);

    void BroadcastToRoom(const std::string &message);

    bool DisconnectGame();

private:
    int _id;
    std::string _name;
    std::string _password;
    std::atomic<int> _creator_id;
    std::atomic<size_t> _player_count;
    std::atomic<bool> _is_starting;
    GameBridge _bridge;

    std::unordered_map<int, int> _clients;
    std::unordered_set<int> _disconnected_clients;
    std::unordered_map<int, std::string> _client_names;
    std::unordered_map<std::string, PendingRequest> _pending_responses;
    std::mutex _room_mutex;

    void SendClientRegistration(int client_id);

    void SendClientDisconnection(int client_id);

    void SendClientReconnection(int client_id);

    void SendClientName(int client_id);

    void RouteLogChannel(const std::string &target, const std::string &raw_line);

    void ListenToGame();
};