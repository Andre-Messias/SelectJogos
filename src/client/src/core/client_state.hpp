#pragma once

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>

enum class ClientScope
{
    IN_LOBBY,
    ROOM_WAITING,
    ROOM_PLAYING
};

struct SentCommand
{
    std::string command;
    std::string arg1;
    std::string arg2;
};

struct ScreenSnapshot
{
    int client_id;
    int room_id;
    std::string room_name;
    std::string game_name;
    ClientScope scope;
    std::vector<std::string> canvas_lines;
    std::vector<std::string> log_lines;
    std::string alert_message;
    std::string input_buffer;
};

class ClientState
{
public:
    ClientState();

    void SetClientId(int id);

    int GetClientId();

    void EnterRoom(int room_id, const std::string &room_name, const std::string &game_name);

    void UpdateRoomMetadata(const std::string &room_name, const std::string &game_name);

    void LeaveRoom();

    void SetScope(ClientScope scope);

    ClientScope GetScope();

    void SetCanvasLines(const std::vector<std::string> &lines);

    void SetCanvasLine(size_t line_index, const std::string &text);

    void ClearCanvas();

    void AddLog(const std::string &formatted_line);

    void SetAlert(const std::string &alert);

    void ClearAlert();

    void AppendInputChar(char c);

    void BackspaceInput();

    std::string ExtractInput();

    void RegisterSentCommand(const std::string &msg_id, const SentCommand &cmd);

    bool PopSentCommand(const std::string &msg_id, SentCommand &out_cmd);

    ScreenSnapshot GetSnapshot();

private:
    static constexpr size_t MAX_LOG_HISTORY = 200;
    static constexpr size_t MAX_CANVAS_LINES = 100;

    int _client_id;
    int _room_id;
    std::string _room_name;
    std::string _game_name;
    ClientScope _scope;

    std::vector<std::string> _canvas_lines;
    std::deque<std::string> _log_lines;
    std::string _alert_message;
    std::string _input_buffer;

    std::unordered_map<std::string, SentCommand> _sent_commands;
    std::mutex _state_mutex;
};