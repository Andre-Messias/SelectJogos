#include "game_room.hpp"
#include "network_utils.hpp"

#include <sstream>
#include <thread>

GameRoom::GameRoom(int id, const std::string& name, const std::string& password, int creator_id, const GameConfig& game_config)
    : _id(id), _name(name), _password(password), _creator_id(creator_id), _bridge(game_config) {}

GameRoom::~GameRoom() {
    DisconnectGame();
}

int GameRoom::GetId() const { return _id; }
std::string GameRoom::GetName() const { return _name; }
std::string GameRoom::GetGameName() const { return _bridge.GetGameName(); }
bool GameRoom::HasPassword() const { return !_password.empty(); }
bool GameRoom::CheckPassword(const std::string& pass) const { return _password == pass; }
bool GameRoom::IsPlaying() const { return _bridge.IsActive(); }
bool GameRoom::IsCreator(int client_id) const { return _creator_id == client_id; }
int GameRoom::GetCreatorId() const { return _creator_id; }

size_t GameRoom::GetPlayerCount() {
    std::lock_guard<std::mutex> lock(_room_mutex);
    return _clients.size();
}

bool GameRoom::HasClient(int client_id) {
    std::lock_guard<std::mutex> lock(_room_mutex);
    return _clients.find(client_id) != _clients.end();
}

void GameRoom::AddClient(int client_id, int socket_fd) {
    std::lock_guard<std::mutex> lock(_room_mutex);
    _clients[client_id] = socket_fd;
    SendClientRegistration(client_id);
}

bool GameRoom::RemoveClient(int client_id) {
    std::lock_guard<std::mutex> lock(_room_mutex);
    _clients.erase(client_id);

    if (_creator_id == client_id && !_clients.empty()) {
        _creator_id = _clients.begin()->first;
        return true;
    }

    return false;
}

bool GameRoom::StartGame(int allocated_port) {
    std::lock_guard<std::mutex> lock(_room_mutex);

    if (!_bridge.Connect(allocated_port)) {
        return false;
    }

    auto self = shared_from_this();
    std::thread([self]() {
        self->ListenToGame();
    }).detach();

    for (const auto& pair : _clients) {
        SendClientRegistration(pair.first);
    }

    return true;
}

void GameRoom::ForwardCommandToGame(int sender_id, int effective_id, const std::string& command, const std::string& msg_id, const std::string& params) {
    std::lock_guard<std::mutex> lock(_room_mutex);
    if (!_bridge.IsActive()) return;

    _pending_responses[msg_id] = sender_id;

    std::string payload = command + " " + msg_id + " " + std::to_string(effective_id);
    if (!params.empty()) {
        payload += " " + params;
    }
    payload += "\n";

    _bridge.Send(payload);
}

void GameRoom::BroadcastToRoom(const std::string& message) {
    std::lock_guard<std::mutex> lock(_room_mutex);
    for (const auto& pair : _clients) {
        NetworkUtils::SendMessage(pair.second, message);
    }
}

bool GameRoom::DisconnectGame() {
    std::lock_guard<std::mutex> lock(_room_mutex);
    _pending_responses.clear();
    return _bridge.Disconnect();
}

void GameRoom::SendClientRegistration(int client_id) {
    if (_bridge.IsActive()) {
        _bridge.Send("ConnectClient internal_init " + std::to_string(client_id) + " " + std::to_string(_id) + "\n");
    }
}

void GameRoom::ListenToGame() {
    std::string buffer;
    while (_bridge.IsActive()) {
        std::string token = _bridge.ReadNextToken(buffer);
        if (token.empty()) {
            break;
        }

        std::istringstream iss(token);
        std::string type, msg_id;
        iss >> type >> msg_id;

        std::lock_guard<std::mutex> lock(_room_mutex);

        if (type == "Response") {
            if (msg_id == "internal_init") continue;

            auto it = _pending_responses.find(msg_id);
            if (it != _pending_responses.end()) {
                int target_client_id = it->second;
                if (_clients.find(target_client_id) != _clients.end()) {
                    NetworkUtils::SendMessage(_clients[target_client_id], token + "\n");
                }
                _pending_responses.erase(it);
            }
        } else {
            for (const auto& pair : _clients) {
                NetworkUtils::SendMessage(pair.second, token + "\n");
            }
        }
    }

    if (DisconnectGame()) {
        BroadcastToRoom("LogChannel 0 \"Game connection closed\"\n");
    }
}