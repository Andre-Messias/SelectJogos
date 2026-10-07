#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <cstdint>
#include "client_state.hpp"
#include "network_client.hpp"

class ProtocolParser
{
public:
    using DirectiveHandler = std::function<void(std::istringstream &iss)>;

    ProtocolParser(ClientState &state, NetworkClient &network, const std::string &help_filepath = "help.txt");

    void HandleLocalInput(const std::string &raw_input);

    void HandleServerMessage(const std::string &line);

private:
    static constexpr uint32_t MAX_MSG_COUNTER = 999999;

    ClientState &_state;
    NetworkClient &_network;
    std::string _help_filepath;
    uint32_t _msg_counter;
    std::unordered_map<std::string, std::string> _command_aliases;
    std::unordered_map<std::string, DirectiveHandler> _screen_directives;

    void RegisterAliases();

    void RegisterScreenDirectives();

    std::string ResolveCommandAlias(const std::string &input_cmd) const;

    void PrintHelpMenu();

    std::string GenerateMsgId();
};