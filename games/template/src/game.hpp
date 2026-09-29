#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <mutex>
#include <unordered_map>
#include <functional>

#include "config.hpp"
#include "player.hpp"
#include "server.hpp"

/// @brief Represents the game logic and state, handling commands received from the network.
class Game {
public:
    /// @brief Constructor initializes the game state and registers command handlers.
    Game();

    /// @brief Processes a message received from a client.
    /// @param socket_fd The file descriptor of the socket from which the message was received.
    /// @param message The raw message string received from the client.
    /// @param server The server instance handling the network communication.
    void ProcessMessage(int socket_fd, const std::string& message, Server& server);

private:
    /// @brief Type definition for the command handler callback function.
    using CommandHandler = std::function<void(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& iss, Server& server)>;

    /// @brief Registry mapping command strings to their corresponding handler functions.
    std::unordered_map<std::string, CommandHandler> _command_registry;
    /// @brief List of players currently connected to the game.
    std::vector<Player> _players;
    /// @brief Number of actions received from players. Used to determine when the game round is complete.
    int _actions_received;
    /// @brief Mutex to protect the game state, ensuring thread safety when processing messages and updating player data.
    std::mutex _game_mutex;

    /// @brief Registers all available commands to their respective member functions.
    void RegisterCommands();

    /// @brief Counts the number of players currently connected to the match.
    /// @return The number of active connected players.
    size_t GetConnectedPlayerCount() const;

    /// @brief Resets the round counter and clears the played state of all registered players.
    void ResetRoundState();

    // Handlers
    /// @brief Handles the "ConnectClient" command, adding a new player to the game if they are not already connected.
    /// @param socket_fd The file descriptor of the socket from which the message was received.
    /// @param msg_id The ID of the message.
    /// @param client_id The ID of the client who sent the message.
    /// @param iss The input string stream containing the message data.
    /// @param server The server instance handling the game.
    void HandleConnectClient(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& /*iss*/, Server& server);

    /// @brief Handles the "DisconnectClient" command, marking an existing player as disconnected and cleaning up pending round actions.
    /// @param socket_fd The file descriptor of the socket from which the message was received.
    /// @param msg_id The ID of the message.
    /// @param client_id The ID of the client who disconnected.
    /// @param iss The input string stream containing the message data.
    /// @param server The server instance handling the game.
    void HandleDisconnectClient(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& /*iss*/, Server& server);

    /// @brief Handles the "ReconnectClient" command, restoring the connected status of a returning player.
    /// @param socket_fd The file descriptor of the socket from which the message was received.
    /// @param msg_id The ID of the message.
    /// @param client_id The ID of the client who reconnected.
    /// @param iss The input string stream containing the message data.
    /// @param server The server instance handling the game.
    void HandleReconnectClient(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& /*iss*/, Server& server);

    /// @brief Handles the "PlayerAction" command, processing the player's action and updating the game state.
    /// @param socket_fd The file descriptor of the socket from which the message was received.
    /// @param msg_id The ID of the message.
    /// @param client_id The ID of the client who sent the message.
    /// @param iss The input string stream containing the message data.
    /// @param server The server instance handling the game.
    void HandlePlayerAction(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& iss, Server& server);

    /// @brief Handles the administrative "ResetRound" command (requires effective ClientID == 0 via ServerAction).
    /// @param socket_fd The file descriptor of the socket from which the message was received.
    /// @param msg_id The ID of the message.
    /// @param client_id The effective ID of the caller (must be 0 for administrative actions).
    /// @param iss The input string stream containing the message data.
    /// @param server The server instance handling the game.
    void HandleResetRound(int socket_fd, const std::string& msg_id, int client_id, std::istringstream& /*iss*/, Server& server);
};