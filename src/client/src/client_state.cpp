#include "client_state.hpp"

ClientState::ClientState()
    : _client_id(-1),
      _room_id(-1),
      _room_name(""),
      _game_name(""),
      _scope(ClientScope::IN_LOBBY) {}

void ClientState::SetClientId(int id) {
    std::lock_guard<std::mutex> lock(_state_mutex);
    _client_id = id;
}

int ClientState::GetClientId() {
    std::lock_guard<std::mutex> lock(_state_mutex);
    return _client_id;
}

void ClientState::EnterRoom(int room_id, const std::string& room_name, const std::string& game_name) {
    std::lock_guard<std::mutex> lock(_state_mutex);
    _room_id = room_id;
    _room_name = room_name;
    _game_name = game_name;
    _scope = ClientScope::ROOM_WAITING;
    _canvas_lines.clear();
}

void ClientState::UpdateRoomMetadata(const std::string& room_name, const std::string& game_name) {
    std::lock_guard<std::mutex> lock(_state_mutex);
    if (!room_name.empty()) {
        _room_name = room_name;
    }
    if (!game_name.empty()) {
        _game_name = game_name;
    }
}

void ClientState::LeaveRoom() {
    std::lock_guard<std::mutex> lock(_state_mutex);
    _room_id = -1;
    _room_name.clear();
    _game_name.clear();
    _scope = ClientScope::IN_LOBBY;
    _canvas_lines.clear();
}

void ClientState::SetScope(ClientScope scope) {
    std::lock_guard<std::mutex> lock(_state_mutex);
    _scope = scope;
    if (scope != ClientScope::ROOM_PLAYING) {
        _canvas_lines.clear();
    }
}

ClientScope ClientState::GetScope() {
    std::lock_guard<std::mutex> lock(_state_mutex);
    return _scope;
}

void ClientState::SetCanvasLines(const std::vector<std::string>& lines) {
    std::lock_guard<std::mutex> lock(_state_mutex);
    _canvas_lines = lines;
    if (_canvas_lines.size() > MAX_CANVAS_LINES) {
        _canvas_lines.resize(MAX_CANVAS_LINES);
    }
}

void ClientState::SetCanvasLine(size_t line_index, const std::string& text) {
    std::lock_guard<std::mutex> lock(_state_mutex);
    if (line_index >= MAX_CANVAS_LINES) {
        return;
    }
    if (line_index >= _canvas_lines.size()) {
        _canvas_lines.resize(line_index + 1, "");
    }
    _canvas_lines[line_index] = text;
}

void ClientState::ClearCanvas() {
    std::lock_guard<std::mutex> lock(_state_mutex);
    _canvas_lines.clear();
}

void ClientState::AddLog(const std::string& formatted_line) {
    std::lock_guard<std::mutex> lock(_state_mutex);
    _log_lines.push_back(formatted_line);
    while (_log_lines.size() > MAX_LOG_HISTORY) {
        _log_lines.pop_front();
    }
}

void ClientState::SetAlert(const std::string& alert) {
    std::lock_guard<std::mutex> lock(_state_mutex);
    _alert_message = alert;
}

void ClientState::ClearAlert() {
    std::lock_guard<std::mutex> lock(_state_mutex);
    _alert_message.clear();
}

void ClientState::AppendInputChar(char c) {
    std::lock_guard<std::mutex> lock(_state_mutex);
    _input_buffer.push_back(c);
}

void ClientState::BackspaceInput() {
    std::lock_guard<std::mutex> lock(_state_mutex);
    while (!_input_buffer.empty()) {
        char last = _input_buffer.back();
        _input_buffer.pop_back();
        // Stop popping once an ASCII char or the leading byte of a UTF-8 sequence is removed.
        if ((static_cast<unsigned char>(last) & 0xC0) != 0x80) {
            break;
        }
    }
}

std::string ClientState::ExtractInput() {
    std::lock_guard<std::mutex> lock(_state_mutex);
    std::string result = _input_buffer;
    _input_buffer.clear();
    return result;
}

void ClientState::RegisterSentCommand(const std::string& msg_id, const SentCommand& cmd) {
    std::lock_guard<std::mutex> lock(_state_mutex);
    _sent_commands[msg_id] = cmd;
}

bool ClientState::PopSentCommand(const std::string& msg_id, SentCommand& out_cmd) {
    std::lock_guard<std::mutex> lock(_state_mutex);
    auto it = _sent_commands.find(msg_id);
    if (it == _sent_commands.end()) {
        return false;
    }
    out_cmd = it->second;
    _sent_commands.erase(it);
    return true;
}

ScreenSnapshot ClientState::GetSnapshot() {
    std::lock_guard<std::mutex> lock(_state_mutex);
    ScreenSnapshot snap;
    snap.client_id = _client_id;
    snap.room_id = _room_id;
    snap.room_name = _room_name;
    snap.game_name = _game_name;
    snap.scope = _scope;
    snap.canvas_lines = _canvas_lines;
    snap.log_lines.assign(_log_lines.begin(), _log_lines.end());
    snap.alert_message = _alert_message;
    snap.input_buffer = _input_buffer;
    return snap;
}