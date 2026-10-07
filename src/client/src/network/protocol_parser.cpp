#include "protocol_parser.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>

ProtocolParser::ProtocolParser(ClientState &state, NetworkClient &network, const std::string &help_filepath)
    : _state(state), _network(network), _help_filepath(help_filepath), _msg_counter(1)
{
    RegisterAliases();
    RegisterScreenDirectives();
}

void ProtocolParser::RegisterAliases()
{
    // Local Client commands
    _command_aliases["help"] = "Help";
    _command_aliases["h"] = "Help";
    _command_aliases["?"] = "Help";

    // Discovery commands
    _command_aliases["lg"] = "ListGames";
    _command_aliases["listgames"] = "ListGames";
    _command_aliases["lr"] = "ListRooms";
    _command_aliases["listrooms"] = "ListRooms";

    // Identity commands
    _command_aliases["nick"] = "SetNick";
    _command_aliases["!nick"] = "SetNick";
    _command_aliases["setnick"] = "SetNick";

    // Room management commands
    _command_aliases["cr"] = "CreateRoom";
    _command_aliases["createroom"] = "CreateRoom";
    _command_aliases["jr"] = "JoinRoom";
    _command_aliases["joinroom"] = "JoinRoom";
    _command_aliases["lv"] = "LeaveRoom";
    _command_aliases["leaveroom"] = "LeaveRoom";

    // Host / Creator commands
    _command_aliases["sg"] = "StartGame";
    _command_aliases["startgame"] = "StartGame";
    _command_aliases["st"] = "StopGame";
    _command_aliases["stopgame"] = "StopGame";
    _command_aliases["sa"] = "ServerAction";
    _command_aliases["serveraction"] = "ServerAction";
    _command_aliases["kp"] = "KickPlayer";
    _command_aliases["kickplayer"] = "KickPlayer";

    // Common In-Game and Admin commands
    _command_aliases["pa"] = "PlayerAction";
    _command_aliases["playeraction"] = "PlayerAction";
    _command_aliases["rr"] = "ResetRound";
    _command_aliases["resetround"] = "ResetRound";
}

void ProtocolParser::RegisterScreenDirectives()
{
    _screen_directives["@SCREEN"] = [this](std::istringstream &iss)
    {
        std::string content;
        std::getline(iss >> std::ws, content);
        std::vector<std::string> lines;
        std::istringstream stream(content);
        std::string segment;
        while (std::getline(stream, segment, '|'))
        {
            size_t first = segment.find_first_not_of("\r\n");
            size_t last = segment.find_last_not_of("\r\n");
            if (first != std::string::npos && last != std::string::npos)
            {
                lines.push_back(segment.substr(first, last - first + 1));
            }
            else
            {
                lines.push_back("");
            }
        }
        _state.SetCanvasLines(lines);
    };

    _screen_directives["@LINE"] = [this](std::istringstream &iss)
    {
        size_t idx;
        if (iss >> idx)
        {
            std::string text;
            std::getline(iss >> std::ws, text);
            _state.SetCanvasLine(idx, text);
        }
    };

    _screen_directives["@CLEAR"] = [this](std::istringstream & /*iss*/)
    {
        _state.ClearCanvas();
    };

    _screen_directives["@ALERT"] = [this](std::istringstream &iss)
    {
        std::string alert_text;
        std::getline(iss >> std::ws, alert_text);
        _state.SetAlert(alert_text);
    };
}

std::string ProtocolParser::ResolveCommandAlias(const std::string &input_cmd) const
{
    std::string lower_cmd = input_cmd;
    std::transform(lower_cmd.begin(), lower_cmd.end(), lower_cmd.begin(),
                   [](unsigned char c)
                   { return static_cast<char>(std::tolower(c)); });

    auto it = _command_aliases.find(lower_cmd);
    if (it != _command_aliases.end())
    {
        return it->second;
    }
    return input_cmd;
}

void ProtocolParser::PrintHelpMenu()
{
    std::ifstream file(_help_filepath);
    if (!file.is_open())
    {
        file.open("client/" + _help_filepath);
    }
    if (!file.is_open())
    {
        file.open("src/client/" + _help_filepath);
    }

    if (!file.is_open())
    {
        _state.SetAlert("[!] ERROR: Could not open help file '" + _help_filepath + "'");
        _state.AddLog("[Error] Help file '" + _help_filepath + "' not found.");
        return;
    }

    _state.ClearAlert();
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty() || line.front() == '#')
        {
            continue;
        }
        _state.AddLog(line);
    }
}

std::string ProtocolParser::GenerateMsgId()
{
    std::string id = "m" + std::to_string(_msg_counter);
    if (_msg_counter >= MAX_MSG_COUNTER)
    {
        _msg_counter = 1;
    }
    else
    {
        ++_msg_counter;
    }
    return id;
}

void ProtocolParser::HandleLocalInput(const std::string &raw_input)
{
    if (raw_input.empty())
    {
        return;
    }

    std::istringstream iss(raw_input);
    std::string raw_command;
    if (!(iss >> raw_command))
    {
        return;
    }

    std::string command = ResolveCommandAlias(raw_command);

    if (command == "Help")
    {
        PrintHelpMenu();
        return;
    }

    std::string remaining_params;
    std::getline(iss >> std::ws, remaining_params);

    // Map custom game shortcuts directly to ServerAction payloads
    if (raw_command == "!start")
    {
        command = "ServerAction";
        remaining_params = "StartGame";
    }
    else if (raw_command == "!name")
    {
        command = "ServerAction";
        remaining_params = "NameTeam " + remaining_params;
    }
    else if (raw_command == "!rank")
    {
        command = "ServerAction";
        remaining_params = "Ranking " + remaining_params;
    }
    else if (raw_command == "!diff")
    {
        command = "ServerAction";
        remaining_params = "JoinRoom " + remaining_params;
    }

    if (command == "ServerAction" && !remaining_params.empty())
    {
        std::istringstream sa_iss(remaining_params);
        std::string sub_cmd, sub_rest;
        if (sa_iss >> sub_cmd)
        {
            std::getline(sa_iss >> std::ws, sub_rest);
            remaining_params = ResolveCommandAlias(sub_cmd);
            if (!sub_rest.empty())
            {
                remaining_params += " " + sub_rest;
            }
        }
    }

    std::string msg_id = GenerateMsgId();
    SentCommand sent_cmd{command, "", ""};

    if (command == "CreateRoom" || command == "JoinRoom")
    {
        std::istringstream param_iss(remaining_params);
        param_iss >> sent_cmd.arg1;
        param_iss >> sent_cmd.arg2;
    }

    _state.RegisterSentCommand(msg_id, sent_cmd);

    std::string payload = command + " " + msg_id;
    if (!remaining_params.empty())
    {
        payload += " " + remaining_params;
    }
    payload += "\n";

    _network.Send(payload);
}

void ProtocolParser::HandleServerMessage(const std::string &line)
{
    std::istringstream iss(line);
    std::string prefix;
    iss >> prefix;

    if (prefix == "Response")
    {
        std::string msg_id, result;
        iss >> msg_id >> result;

        std::string reason;
        std::getline(iss >> std::ws, reason);

        SentCommand cmd;
        bool tracked = _state.PopSentCommand(msg_id, cmd);

        if (result == "Success")
        {
            _state.ClearAlert();
            if (tracked)
            {
                if (cmd.command == "CreateRoom")
                {
                    int room_id = -1;
                    try
                    {
                        room_id = std::stoi(reason);
                    }
                    catch (...)
                    {
                    }
                    _state.EnterRoom(room_id, cmd.arg1, cmd.arg2);
                    _state.AddLog("[System] Room '" + cmd.arg1 + "' created (ID: " + std::to_string(room_id) + ").");
                }
                else if (cmd.command == "JoinRoom")
                {
                    int room_id = -1;
                    try
                    {
                        room_id = std::stoi(cmd.arg1);
                    }
                    catch (...)
                    {
                    }
                    _state.EnterRoom(room_id, "", "");
                    _state.AddLog("[System] Joined room ID " + cmd.arg1 + ".");
                }
                else if (cmd.command == "LeaveRoom")
                {
                    _state.LeaveRoom();
                    _state.AddLog("[System] Left the room.");
                }
                else if (cmd.command == "StartGame")
                {
                    _state.SetScope(ClientScope::ROOM_PLAYING);
                }
                else if (cmd.command == "StopGame")
                {
                    _state.SetScope(ClientScope::ROOM_WAITING);
                    _state.ClearCanvas();
                }
            }
            if (!reason.empty() && cmd.command != "CreateRoom")
            {
                _state.AddLog("[Response] " + reason);
            }
        }
        else
        {
            std::string err_msg = reason.empty() ? "Unknown error" : reason;
            if (err_msg.size() >= 2 && err_msg.front() == '"' && err_msg.back() == '"')
            {
                err_msg = err_msg.substr(1, err_msg.length() - 2);
            }
            _state.SetAlert("[!] ERROR: " + err_msg);
            _state.AddLog("[Error] " + err_msg);
        }
    }
    else if (prefix == "LogChannel")
    {
        std::string channel;
        iss >> channel;

        std::string message;
        std::getline(iss >> std::ws, message);

        if (message.size() >= 2 && message.front() == '"' && message.back() == '"')
        {
            message = message.substr(1, message.length() - 2);
        }

        // Dispatch '@' screen directives via the directive registry
        if (!message.empty() && message.front() == '@')
        {
            std::istringstream dir_iss(message);
            std::string directive;
            if (dir_iss >> directive)
            {
                auto it = _screen_directives.find(directive);
                if (it != _screen_directives.end())
                {
                    it->second(dir_iss);
                    return;
                }
            }
        }

        std::string formatted_tag = "[System]";
        if (channel == "All")
        {
            formatted_tag = "[Game]";
        }
        else if (channel != "0")
        {
            formatted_tag = "[Private]";
        }
        else
        {
            if (message.find("started in room") != std::string::npos)
            {
                _state.SetScope(ClientScope::ROOM_PLAYING);
                formatted_tag = "[Room]";
            }
            else if (message.find("forcibly stopped") != std::string::npos ||
                     message.find("Game connection closed") != std::string::npos)
            {
                _state.SetScope(ClientScope::ROOM_WAITING);
                _state.ClearCanvas();
                formatted_tag = "[Room]";
            }
            else if (message.find("You have been kicked") != std::string::npos)
            {
                _state.LeaveRoom();
                _state.SetAlert("[!] " + message);
            }
            else if (message.find("joined the room") != std::string::npos ||
                     message.find("left the room") != std::string::npos ||
                     message.find("is now the room creator") != std::string::npos)
            {
                formatted_tag = "[Room]";
            }
        }

        if (channel == "0" && message.find("Connected with ClientID") != std::string::npos)
        {
            size_t pos = message.find("ClientID");
            if (pos != std::string::npos)
            {
                try
                {
                    int cid = std::stoi(message.substr(pos + 9));
                    _state.SetClientId(cid);
                }
                catch (...)
                {
                }
            }
        }

        _state.AddLog(formatted_tag + " " + message);
    }
    else
    {
        _state.AddLog(line);
    }
}