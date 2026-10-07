#include "room_manager.hpp"

#include <iostream>

RoomManager::RoomManager(const std::string& config_path) {
    _available_games = ConfigLoader::Load(config_path);
}

std::string RoomManager::FormatGamesList() const {
    if (_available_games.empty()) {
        return " \"No games configured\"";
    }

    std::string result;
    for (const auto& pair : _available_games) {
        std::string mode_str = (pair.second.mode == GameMode::LOCAL) ? "[Local]" : "[Remote]";
        result += " | " + pair.first + mode_str;
    }
    return result;
}

std::string RoomManager::FormatRoomsList() {
    std::lock_guard<std::mutex> lock(_manager_mutex);

    if (_rooms.empty()) {
        return " \"No rooms available\"";
    }

    std::string result;
    for (const auto& pair : _rooms) {
        const auto& room = pair.second;
        std::string privacy = room->HasPassword() ? "[Private]" : "[Public]";
        std::string status = room->IsPlaying() ? "[Playing]" : "[Waiting]";

        result += " | ID:" + std::to_string(room->GetId()) +
            " Name:" + room->GetName() +
            " Game:" + room->GetGameName() +
            " Players:" + std::to_string(room->GetPlayerCount()) +
            " " + privacy + status;
    }
    return result;
}

int RoomManager::CreateRoom(int creator_id, int creator_fd, const std::string& room_name,
    const std::string& game_name, const std::string& password, std::string& out_error, const std::string& nickname) {
    std::lock_guard<std::mutex> lock(_manager_mutex);

    if (_client_to_room.find(creator_id) != _client_to_room.end()) {
        out_error = "Client already in a room";
        return -1;
    }

    auto game_it = _available_games.find(game_name);
    if (game_it == _available_games.end()) {
        out_error = "Game not found in game.config";
        return -1;
    }

    int room_id = _room_id_gen.AcquireId();
    if (room_id == -1) {
        out_error = "Maximum number of rooms reached";
        return -1;
    }

    auto new_room = std::make_shared<GameRoom>(room_id, room_name, password, creator_id, game_it->second);
    new_room->AddClient(creator_id, creator_fd, nickname);

    _rooms[room_id] = new_room;
    _client_to_room[creator_id] = room_id;

    std::cout << "[Lobby] ClientID " << creator_id << " created room '" << room_name
        << "' for game '" << game_name << "' (ID: " << room_id << ")\n";
    return room_id;
}

bool RoomManager::JoinRoom(int client_id, int client_fd, int room_id, const std::string& password, std::string& out_error, const std::string& nickname) {
    std::lock_guard<std::mutex> lock(_manager_mutex);

    if (_client_to_room.find(client_id) != _client_to_room.end()) {
        out_error = "Client already in a room";
        return false;
    }

    auto it = _rooms.find(room_id);
    if (it == _rooms.end()) {
        out_error = "Room not found";
        return false;
    }

    auto& room = it->second;
    if (room->HasPassword() && !room->CheckPassword(password)) {
        out_error = "Incorrect password";
        return false;
    }

    room->AddClient(client_id, client_fd, nickname);
    _client_to_room[client_id] = room_id;

    std::cout << "[Lobby] ClientID " << client_id << " joined room ID " << room_id << "\n";
    room->BroadcastToRoom("LogChannel 0 \"ClientID " + std::to_string(client_id) + " joined the room\"\n");
    return true;
}

bool RoomManager::RemoveClientFromRoom(int client_id, bool permanent) {
    std::lock_guard<std::mutex> lock(_manager_mutex);

    auto it = _client_to_room.find(client_id);
    if (it == _client_to_room.end()) {
        return false;
    }

    int room_id = it->second;
    _client_to_room.erase(it);

    auto room_it = _rooms.find(room_id);
    if (room_it != _rooms.end()) {
        auto& room = room_it->second;
        bool creator_changed = room->RemoveClient(client_id, permanent);

        std::cout << "[Lobby] ClientID " << client_id << " left room ID " << room_id << "\n";

        if (room->GetPlayerCount() == 0) {
            room->DisconnectGame();
            _rooms.erase(room_it);
            _room_id_gen.ReleaseId(room_id);
            std::cout << "[Lobby] Room ID " << room_id << " is empty and was removed.\n";
        } else {
            room->BroadcastToRoom("LogChannel 0 \"ClientID " + std::to_string(client_id) + " left the room\"\n");
            if (creator_changed) {
                int new_creator = room->GetCreatorId();
                std::cout << "[Lobby] Room ID " << room_id << " ownership transferred to ClientID " << new_creator << "\n";
                room->BroadcastToRoom("LogChannel 0 \"ClientID " + std::to_string(new_creator) + " is now the room creator\"\n");
            }
        }
    }
    return true;
}

std::shared_ptr<GameRoom> RoomManager::GetClientRoom(int client_id) {
    std::lock_guard<std::mutex> lock(_manager_mutex);

    auto it = _client_to_room.find(client_id);
    if (it == _client_to_room.end()) {
        return nullptr;
    }

    auto room_it = _rooms.find(it->second);
    return (room_it != _rooms.end()) ? room_it->second : nullptr;
}

const GameConfig* RoomManager::GetGameConfig(const std::string& game_name) const {
    auto it = _available_games.find(game_name);
    return (it != _available_games.end()) ? &(it->second) : nullptr;
}