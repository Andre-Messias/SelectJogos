#include "game_room.hpp"
#include "network_utils.hpp"

#include <sstream>
#include <thread>

GameRoom::GameRoom(int id, const std::string& name, const std::string& password, int creator_id, const GameConfig& game_config)
    : _id(id),
      _name(name),
      _password(password),
      _creator_id(creator_id),
      _player_count(0),
      _is_starting(false),
      _bridge(game_config) {}

GameRoom::~GameRoom() {
    DisconnectGame();
}

int GameRoom::GetId() const { return _id; }
std::string GameRoom::GetName() const { return _name; }
std::string GameRoom::GetGameName() const { return _bridge.GetGameName(); }
bool GameRoom::HasPassword() const { return !_password.empty(); }
bool GameRoom::CheckPassword(const std::string& pass) const { return _password == pass; }
bool GameRoom::IsPlaying() const { return _bridge.IsActive(); }
bool GameRoom::IsCreator(int client_id) const { return _creator_id.load() == client_id; }
int GameRoom::GetCreatorId() const { return _creator_id.load(); }
size_t GameRoom::GetPlayerCount() const { return _player_count.load(); }

bool GameRoom::HasClient(int client_id) {
    std::lock_guard<std::mutex> lock(_room_mutex);
    return _clients.find(client_id) != _clients.end();
}

void GameRoom::AddClient(int client_id, int socket_fd) {
    std::lock_guard<std::mutex> lock(_room_mutex);
    _clients[client_id] = socket_fd;
    _player_count.store(_clients.size());

    if (_bridge.IsActive()) {
        if (_disconnected_clients.erase(client_id) > 0) {
            SendClientReconnection(client_id);
        } else {
            SendClientRegistration(client_id);
        }
    }
}

bool GameRoom::RemoveClient(int client_id) {
    std::lock_guard<std::mutex> lock(_room_mutex);
    if (_clients.erase(client_id) > 0) {
        _player_count.store(_clients.size());
        if (_bridge.IsActive()) {
            _disconnected_clients.insert(client_id);
            SendClientDisconnection(client_id);
        }
    }

    if (_creator_id.load() == client_id && !_clients.empty()) {
        _creator_id.store(_clients.begin()->first);
        return true;
    }

    return false;
}

void GameRoom::SetClientName(int client_id, const std::string& name) {
    std::lock_guard<std::mutex> lock(_room_mutex);
    _client_names[client_id] = name;
    if (_bridge.IsActive() && _clients.count(client_id)) {
        _bridge.Send("SetPlayerName internal_name " + std::to_string(client_id) + " " + name + "\n");
    }
}

bool GameRoom::StartGame(int allocated_port) {
    bool expected = false;
    if (!_is_starting.compare_exchange_strong(expected, true)) {
        return false;
    }

    if (!_bridge.Connect(allocated_port)) {
        _is_starting.store(false);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(_room_mutex);
        _disconnected_clients.clear();

        auto self = shared_from_this();
        std::thread([self]() {
            self->ListenToGame();
        }).detach();

        for (const auto& pair : _clients) {
            SendClientRegistration(pair.first);
        }
    }

    _is_starting.store(false);
    return true;
}

void GameRoom::ForwardCommandToGame(int sender_id, int effective_id, const std::string& command, const std::string& msg_id, const std::string& params) {
    std::lock_guard<std::mutex> lock(_room_mutex);
    if (!_bridge.IsActive()) return;

    std::string internal_msg_id = std::to_string(sender_id) + "_" + msg_id;
    _pending_responses[internal_msg_id] = PendingRequest{sender_id, msg_id};

    std::string payload = command + " " + internal_msg_id + " " + std::to_string(effective_id);
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
    _disconnected_clients.clear();
    return _bridge.Disconnect();
}

void GameRoom::SendClientRegistration(int client_id) {
    if (_bridge.IsActive()) {
        std::string payload = "ConnectClient internal_init " + std::to_string(client_id) + " " + std::to_string(_id);
        auto name_it = _client_names.find(client_id);
        if (name_it != _client_names.end()) {
            payload += " " + name_it->second;
        }
        _bridge.Send(payload + "\n");
    }
}

void GameRoom::SendClientDisconnection(int client_id) {
    if (_bridge.IsActive()) {
        _bridge.Send("DisconnectClient internal_disc " + std::to_string(client_id) + "\n");
    }
}

void GameRoom::SendClientReconnection(int client_id) {
    if (_bridge.IsActive()) {
        _bridge.Send("ReconnectClient internal_recon " + std::to_string(client_id) + "\n");
    }
}

void GameRoom::RouteLogChannel(const std::string& target, const std::string& raw_line) {
    if (target.empty() || target == "All") {
        for (const auto& pair : _clients) {
            NetworkUtils::SendMessage(pair.second, raw_line);
        }
        return;
    }

    std::string clean_target = target;
    if (!clean_target.empty() && clean_target.front() == '[') {
        clean_target.erase(0, 1);
    }
    if (!clean_target.empty() && clean_target.back() == ']') {
        clean_target.pop_back();
    }

    std::istringstream target_stream(clean_target);
    std::string id_str;
    while (std::getline(target_stream, id_str, ',')) {
        try {
            int target_id = std::stoi(id_str);
            auto client_it = _clients.find(target_id);
            if (client_it != _clients.end()) {
                NetworkUtils::SendMessage(client_it->second, raw_line);
            }
        } catch (...) {
            continue;
        }
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
        std::string type, second_token;
        iss >> type >> second_token;

        std::lock_guard<std::mutex> lock(_room_mutex);

        if (type == "Response") {
            const std::string& internal_msg_id = second_token;
            if (internal_msg_id == "internal_init" ||
                internal_msg_id == "internal_disc" ||
                internal_msg_id == "internal_recon" ||
                internal_msg_id == "internal_name") {
                continue;
            }

            auto it = _pending_responses.find(internal_msg_id);
            if (it != _pending_responses.end()) {
                int target_client_id = it->second.sender_id;
                std::string original_msg_id = it->second.original_msg_id;

                std::string rest_of_line;
                std::getline(iss, rest_of_line);

                auto client_it = _clients.find(target_client_id);
                if (client_it != _clients.end()) {
                    NetworkUtils::SendMessage(client_it->second, "Response " + original_msg_id + rest_of_line + "\n");
                }
                _pending_responses.erase(it);
            }
        } else if (type == "LogChannel") {
            RouteLogChannel(second_token, token + "\n");
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