#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

#include "game_config.hpp"
#include "game_room.hpp"
#include "id_generator.hpp"

class RoomManager
{
public:
    explicit RoomManager(const std::string &config_path);

    std::string FormatGamesList() const;

    std::string FormatRoomsList();

    int CreateRoom(int creator_id, int creator_fd, const std::string &room_name,
                   const std::string &game_name, const std::string &password, std::string &out_error, const std::string &nickname = "");

    bool JoinRoom(int client_id, int client_fd, int room_id, const std::string &password, std::string &out_error, const std::string &nickname = "");

    bool RemoveClientFromRoom(int client_id, bool permanent = false);

    std::shared_ptr<GameRoom> GetClientRoom(int client_id);

    const GameConfig *GetGameConfig(const std::string &game_name) const;

private:
    std::unordered_map<std::string, GameConfig> _available_games;
    std::unordered_map<int, std::shared_ptr<GameRoom>> _rooms;
    std::unordered_map<int, int> _client_to_room;
    IdGenerator _room_id_gen;
    std::mutex _manager_mutex;
};