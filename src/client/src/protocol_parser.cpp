#include "protocol_parser.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>

ProtocolParser::ProtocolParser(ClientState& state, NetworkClient& network)
    : _state(state), _network(network), _msg_counter(1) {}

std::string ProtocolParser::GenerateMsgId() {
    return "m" + std::to_string(_msg_counter++);
}

void ProtocolParser::HandleLocalInput(const std::string& raw_input) {
    if (raw_input.empty()) {
        return;
    }

    std::istringstream iss(raw_input);
    std::string command;
    iss >> command;

    std::string remaining_params;
    std::getline(iss >> std::ws, remaining_params);

    std::string msg_id = GenerateMsgId();
    SentCommand sent_cmd{command, "", ""};

    // Track state transitions based on local commands
    if (command == "CreateRoom" || command == "JoinRoom") {
        std::istringstream param_iss(remaining_params);
        param_iss >> sent_cmd.arg1; // RoomName or RoomID
        param_iss >> sent_cmd.arg2; // GameName (if CreateRoom)
    }

    _state.RegisterSentCommand(msg_id, sent_cmd);

    // Format and transmit to Lobby: <Command> <MsgID> [Parameters...][cite: 1]
    std::string payload = command + " " + msg_id;
    if (!remaining_params.empty()) {
        payload += " " + remaining_params;
    }
    payload += "\n";

    _network.Send(payload);
}

void ProtocolParser::HandleServerMessage(const std::string& line) {
    std::istringstream iss(line);
    std::string prefix;
    iss >> prefix;

    if (prefix == "Response") {
        // Format: Response <MsgID> <Success|Fail> [Data_or_Reason][cite: 1]
        std::string msg_id, result;
        iss >> msg_id >> result;

        std::string reason;
        std::getline(iss >> std::ws, reason);

        SentCommand cmd;
        bool tracked = _state.PopSentCommand(msg_id, cmd);

        if (result == "Success") {
            _state.ClearAlert();
            if (tracked) {
                if (cmd.command == "CreateRoom") {
                    // reason contains the generated RoomID[cite: 1]
                    int room_id = -1;
                    try { room_id = std::stoi(reason); } catch(...) {}
                    _state.EnterRoom(room_id, cmd.arg1, cmd.arg2);
                    _state.AddLog("[Sistema] Sala '" + cmd.arg1 + "' criada com sucesso (ID: " + std::to_string(room_id) + ").");
                } else if (cmd.command == "JoinRoom") {
                    int room_id = -1;
                    try { room_id = std::stoi(cmd.arg1); } catch(...) {}
                    _state.EnterRoom(room_id, "", "");
                    _state.AddLog("[Sistema] Você entrou na sala ID " + cmd.arg1 + ".");
                } else if (cmd.command == "LeaveRoom") {
                    _state.LeaveRoom();
                    _state.AddLog("[Sistema] Você saiu da sala.");
                } else if (cmd.command == "StartGame") {
                    _state.SetScope(ClientScope::ROOM_PLAYING);
                } else if (cmd.command == "StopGame") {
                    _state.SetScope(ClientScope::ROOM_WAITING);
                    _state.ClearCanvas();
                }
            }
            if (!reason.empty() && cmd.command != "CreateRoom") {
                _state.AddLog("[Resposta] " + reason);
            }
        } else {
            // Failure response[cite: 1]
            std::string err_msg = reason.empty() ? "Erro desconhecido" : reason;
            // Remove surrounding quotes if present
            if (err_msg.front() == '"' && err_msg.back() == '"') {
                err_msg = err_msg.substr(1, err_msg.length() - 2);
            }
            _state.SetAlert("[!] ERRO: " + err_msg);
            _state.AddLog("[Erro] " + err_msg);
        }
    } 
    else if (prefix == "LogChannel") {
        // Format: LogChannel <Channel> "<Message>"[cite: 1, 2]
        std::string channel;
        iss >> channel;

        std::string message;
        std::getline(iss >> std::ws, message);

        if (!message.empty() && message.front() == '"' && message.back() == '"') {
            message = message.substr(1, message.length() - 2);
        }

        // Handle optional custom game screen sub-protocol directives
        if (message.rfind("@SCREEN ", 0) == 0) {
            std::string content = message.substr(8);
            std::vector<std::string> lines;
            std::istringstream stream(content);
            std::string segment;
            while (std::getline(stream, segment, '|')) {
                // Trim spaces
                segment.erase(0, segment.find_first_not_of(" \t\r\n"));
                segment.erase(segment.find_last_not_of(" \t\r\n") + 1);
                lines.push_back(segment);
            }
            _state.SetCanvasLines(lines);
            return;
        } 
        else if (message.rfind("@CLEAR", 0) == 0) {
            _state.ClearCanvas();
            return;
        } 
        else if (message.rfind("@ALERT ", 0) == 0) {
            _state.SetAlert(message.substr(7));
            return;
        }

        // Standard log formatting based on channel[cite: 1, 2]
        std::string formatted_tag = "[Sistema]";
        if (channel == "All") {
            formatted_tag = "[Jogo]";
            if (message.find("started in room") != std::string::npos) {
                _state.SetScope(ClientScope::ROOM_PLAYING);
            } else if (message.find("forcibly stopped") != std::string::npos || message.find("Game connection closed") != std::string::npos) {
                _state.SetScope(ClientScope::ROOM_WAITING);
                _state.ClearCanvas();
            }
        } else if (channel != "0") {
            formatted_tag = "[Privado]";
        } else {
            if (message.find("joined the room") != std::string::npos || message.find("left the room") != std::string::npos) {
                formatted_tag = "[Sala]";
            }
        }

        // Extract ClientID on initial connection welcome message[cite: 7]
        if (channel == "0" && message.find("Connected with ClientID") != std::string::npos) {
            size_t pos = message.find("ClientID");
            if (pos != std::string::npos) {
                try {
                    int cid = std::stoi(message.substr(pos + 9));
                    _state.SetClientId(cid);
                } catch(...) {}
            }
        }

        _state.AddLog(formatted_tag + " " + message);
    } 
    else {
        _state.AddLog(line);
    }
}