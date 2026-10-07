#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <mutex>
#include <unordered_map>
#include <functional>

#include "player.hpp"
#include "server.hpp"
#include "board.hpp"
#include "room.hpp"

/// @brief Represents the game logic and state, handling commands received from the network.
class Game
{
public:
    /// @brief Constructor initializes the game state and registers command handlers.
    Game();

    /// @brief Destructor cleanly shuts down any background threads.
    ~Game();

    /// @brief Starts the background tasks for the game (e.g. inactivity checker).
    void Start(Server &server);

    /// @brief Processes a message received from a client.
    /// @param socket_fd The file descriptor of the socket from which the message was received.
    /// @param message The raw message string received from the client.
    /// @param server The server instance handling the network communication.
    void ProcessMessage(int socket_fd, const std::string &message, Server &server);

    /// @brief Called when a TCP connection to this game process closes (normally the Lobby's GameBridge).
    /// Marks every player registered through that socket as disconnected and resets emptied rooms.
    /// If the process was spawned by a Lobby that has since died (re-parented to PID 1), the process exits
    /// instead of lingering as an orphan.
    /// @param socket_fd The file descriptor of the closed connection.
    /// @param server The server instance (used to notify any remaining connections).
    void HandleSocketDisconnect(int socket_fd, Server &server);

private:
    /// @brief Type definition for the command handler callback function.
    using CommandHandler = std::function<void(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server)>;

    /// @brief Registry mapping command strings to their corresponding handler functions.
    std::unordered_map<std::string, CommandHandler> _command_registry;
    /// @brief List of players currently connected to the game.
    std::vector<Player> _players;
    /// @brief Mutex to protect the game state, ensuring thread safety when processing messages and updating player data.
    std::mutex _game_mutex;

    int _active_room_id = 1;
    std::vector<Room> _rooms;

    void InitRooms();

    /// @brief Registers all available commands to their respective member functions.
    void RegisterCommands();

    /// @brief Thread loop that kicks players who have been inactive for more than 5 minutes.
    void InactivityCheckerLoop(Server &server);

    std::thread _inactivity_thread;
    std::atomic<bool> _is_running;

    /// @brief Returns a formatted string of all players currently in a room.
    std::string GetRoomPlayersString(int room_id);

    /// @brief Renders the full screen (players header, status line for the room's state, and board)
    /// and broadcasts it as an @SCREEN update. Single source of truth for the status-line texts.
    void BroadcastRoomScreen(Room &room, Server &server);

    /// @brief Finds a difficulty room by ID, or nullptr.
    Room *FindRoom(int room_id);

    /// @brief Finds a player by ID, or nullptr.
    Player *FindPlayer(int client_id);

    /// @brief Removes a player from their room (room_id = -1). Resets the room if nobody connected is left,
    /// otherwise redraws the screen for the remaining players. Caller sets the connection flag.
    void DetachPlayerFromRoom(Player &p, Server &server);

    // Handlers
    /// @brief Handles the "ConnectClient" command, adding a new player to the game if they are not already connected.
    void HandleConnectClient(int socket_fd, const std::string &msg_id, int client_id, std::istringstream & /*iss*/, Server &server);

    /// @brief Handles the "DisconnectClient" command, marking an existing player as disconnected.
    void HandleDisconnectClient(int socket_fd, const std::string &msg_id, int client_id, std::istringstream & /*iss*/, Server &server);

    /// @brief Handles the "ReconnectClient" command, restoring the connected status of a returning player.
    void HandleReconnectClient(int socket_fd, const std::string &msg_id, int client_id, std::istringstream & /*iss*/, Server &server);

    /// @brief Handles the "SetPlayerName" command sent by the Lobby when a player changes nickname mid-session.
    void HandleSetPlayerName(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server);

    /// @brief Handles a player joining a specific room.
    void HandleJoinRoom(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server);

    /// @brief Handles a player's move on the board (reveal, flag, unflag).
    void HandlePlayerAction(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server);

    /// @brief Starts the game in a room (transitions from LOBBY to PLAYING).
    void HandleStartGame(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server);

    /// @brief Handles the host naming the team after a win.
    void HandleNameTeam(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server);

    /// @brief Displays the leaderboard.
    void HandleRanking(int socket_fd, const std::string &msg_id, int client_id, std::istringstream &iss, Server &server);

    /// @brief Broadcasts a message to all players currently in a specific room.
    void BroadcastToRoom(int room_id, const std::string &message, Server &server);

    /// @brief Sends a message to a specific player via LogChannel
    void SendToPlayer(int client_id, const std::string &message, Server &server);
};