#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <sstream>
#include <cstdint>
#include "client_state.hpp"
#include "network_client.hpp"

/// @brief Translates user text input into Lobby protocol requests and parses inbound server lines.
class ProtocolParser {
    public:
        /// @brief Callback type for handling @ screen directives inside LogChannel messages.
        using DirectiveHandler = std::function<void(std::istringstream& iss)>;

        /// @brief Constructs a ProtocolParser linked to the client state and network interface.
        /// @param state Reference to the thread-safe client state model.
        /// @param network Reference to the TCP network client.
        /// @param help_filepath Path to the external text file containing the help menu.
        ProtocolParser(ClientState& state, NetworkClient& network, const std::string& help_filepath = "help.txt");

        /// @brief Formats a raw user command string, resolves shorthand aliases, injects a MsgID, and sends it to the Lobby.
        /// @param raw_input The command string typed by the user (e.g., "pa 250", "help", or "CreateRoom Arena1 HigherLower").
        void HandleLocalInput(const std::string& raw_input);

        /// @brief Parses an inbound newline-delimited line received from the Lobby server.
        /// @param line The raw line from the TCP stream.
        void HandleServerMessage(const std::string& line);

    private:
        /// @brief Maximum value for the message ID counter before wrapping back to 1.
        static constexpr uint32_t MAX_MSG_COUNTER = 999999;

        /// @brief Reference to the thread-safe client state model.
        ClientState& _state;
        /// @brief Reference to the TCP network client.
        NetworkClient& _network;
        /// @brief Path to the external help text file.
        std::string _help_filepath;
        /// @brief Bounded counter for generating unique message IDs without overflow issues.
        uint32_t _msg_counter;
        /// @brief Maps lowercase full commands and shorthand aliases to their canonical case-sensitive protocol names.
        std::unordered_map<std::string, std::string> _command_aliases;
        /// @brief Maps '@' visual directives (e.g., "@SCREEN", "@CLEAR") to their respective handler functions.
        std::unordered_map<std::string, DirectiveHandler> _screen_directives;

        /// @brief Initializes the dictionary of case-insensitive command aliases and shorthands.
        void RegisterAliases();

        /// @brief Initializes the registry of visual '@' screen directives.
        void RegisterScreenDirectives();

        /// @brief Resolves a user-typed command token into its canonical protocol command name.
        /// @param input_cmd The raw command token typed by the user.
        /// @return The canonical command string (e.g., "pa" -> "PlayerAction"), or input_cmd if no alias matches.
        std::string ResolveCommandAlias(const std::string& input_cmd) const;

        /// @brief Reads the external help file from disk and displays its lines in the UI log feed.
        void PrintHelpMenu();

        /// @brief Generates a unique message ID token in the range ["m1", "m999999"].
        /// @return The generated message ID string.
        std::string GenerateMsgId();
};