#pragma once

#include <string>
#include "client_state.hpp"
#include "network_client.hpp"

/// @brief Translates user text input into Lobby protocol requests and parses inbound server lines.
class ProtocolParser {
    public:
        /// @brief Constructs a ProtocolParser linked to the client state and network interface.
        ProtocolParser(ClientState& state, NetworkClient& network);

        /// @brief Formats a raw user command string, injects an auto-generated MsgID, and sends it to the Lobby.
        /// @param raw_input The command string typed by the user.
        void HandleLocalInput(const std::string& raw_input);

        /// @brief Parses an inbound newline-delimited line received from the Lobby server.
        /// @param line The raw line from the TCP stream.
        void HandleServerMessage(const std::string& line);

    private:
        /// @brief Reference to the thread-safe client state model.
        ClientState& _state;
        /// @brief Reference to the TCP network client.
        NetworkClient& _network;
        /// @brief Counter for generating unique message IDs.
        unsigned long _msg_counter;

        /// @brief Generates a unique message ID token (e.g., "m1", "m2").
        std::string GenerateMsgId();
};