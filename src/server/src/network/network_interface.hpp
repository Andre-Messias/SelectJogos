#pragma once

#include <string>
#include <sstream>
#include <unordered_map>
#include <mutex>
#include <functional>

#include "id_generator.hpp"
#include "room_manager.hpp"

/// @brief Maximum accepted length (in bytes) for a client nickname.
constexpr size_t MAX_NICKNAME_LENGTH = 16;

/// @brief Encapsulates all metadata and stream payload for a parsed client command.
struct CommandContext {
    /// @brief The ID of the client sending the command.
    int client_id;
    /// @brief The socket file descriptor of the client sending the command.
    int socket_fd;
    /// @brief The command name extracted from the client's message.
    std::string msg_id;
    /// @brief The remaining message content after the command name, used for parsing arguments.
    std::istringstream& iss;
};

/// @brief Manages the TCP server connections and dispatches protocol commands to the RoomManager.
class NetworkInterface {
public:
    /// @brief Constructs a new NetworkInterface instance.
    /// @param lobby_port The port on which the lobby server will listen for incoming connections.
    /// @param config_path The path to the game configuration file.
    NetworkInterface(int lobby_port, const std::string& config_path = "game.config");

    /// @brief Destructor that cleans up resources and closes the server socket.
    ~NetworkInterface();

    /// @brief Starts the lobby TCP server loop.
    void Start();

private:
    /// @brief Defines the type for command handler functions that process client commands.
    using CommandHandler = std::function<void(CommandContext& ctx)>;

    /// @brief The port on which the lobby server listens for incoming connections.
    int _port;
    /// @brief The file descriptor for the server socket.
    int _server_fd;
    /// @brief Indicates whether the server is currently running and accepting connections.
    bool _is_running;

    /// @brief Generates unique IDs.
    IdGenerator _client_id_gen;
    /// @brief Manages the game catalog, active rooms, and client-to-room associations.
    RoomManager _room_manager;

    /// @brief Maps command names to their corresponding handler functions for processing client commands.
    std::unordered_map<std::string, CommandHandler> _command_registry;
    /// @brief Maps client IDs to their corresponding socket file descriptors for communication.
    std::unordered_map<int, int> _client_sockets;
    /// @brief Maps client IDs to their display usernames.
    std::unordered_map<int, std::string> _client_usernames;
    /// @brief Mutex to protect access to the client sockets map, ensuring thread safety.
    std::mutex _clients_mutex;

    /// @brief Handles a new client connection.
    /// @param client_fd The file descriptor of the new client's socket.
    /// @param client_id The unique ID assigned to the new client.
    void HandleClient(int client_fd, int client_id);

    /// @brief Processes a message received from a client.
    /// @param client_id The unique ID of the client.
    /// @param socket_fd The file descriptor of the client's socket.
    /// @param message The message received from the client.
    void ProcessMessage(int client_id, int socket_fd, const std::string& message);

    /// @brief Registers all supported commands and their corresponding handler functions.
    void RegisterCommands();

    // Command Handlers
    
    /// @brief Lists all available games to the client.
    /// @param ctx The context containing client information and message data.
    void HandleListGames(CommandContext& ctx);

    /// @brief Lists all active rooms and their states to the client.
    /// @param ctx The context containing client information and message data.
    void HandleListRooms(CommandContext& ctx);

    /// @brief Sets the client's display nickname (SetNick <MsgID> <Nickname>).
    /// @param ctx The context containing client information and message data.
    void HandleSetNick(CommandContext& ctx);

    /// @brief Returns the client's nickname, or an empty string if none was set.
    /// @param client_id The ID of the client.
    std::string GetNickname(int client_id);

    /// @brief Pushes the client's nickname (if any) to the room the client is currently in.
    /// @param client_id The ID of the client.
    void ApplyNicknameToRoom(int client_id);

    /// @brief Creates a new game room.
    /// @param ctx The context containing client information and message data.
    void HandleCreateRoom(CommandContext& ctx);

    /// @brief Allows a client to join an existing game room.
    /// @param ctx The context containing client information and message data.
    void HandleJoinRoom(CommandContext& ctx);

    /// @brief Allows a client to leave their current game room.
    /// @param ctx The context containing client information and message data.
    void HandleLeaveRoom(CommandContext& ctx);

    /// @brief Starts the game in the room where the client is currently located.
    /// @param ctx The context containing client information and message data.
    void HandleStartGame(CommandContext& ctx);

    /// @brief Stops the currently running game.
    /// @param ctx The context containing client information and message data.
    void HandleStopGame(CommandContext& ctx);

    /// @brief Executes a server-level action in the game.
    /// @param ctx The context containing client information and message data.
    void HandleServerAction(CommandContext& ctx);

    /// @brief Kicks a player from the room where the client is currently located.
    /// @param ctx The context containing client information and message data.
    void HandleKickPlayer(CommandContext& ctx);
};