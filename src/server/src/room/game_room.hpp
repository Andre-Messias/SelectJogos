#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <atomic>

#include "game_config.hpp"
#include "game_bridge.hpp"

/// @brief Tracks a client request awaiting a reply from the game process.
struct PendingRequest {
    /// @brief The ID of the client who originated the request.
    int sender_id;
    /// @brief The original message ID sent by the client before internal prefixing.
    std::string original_msg_id;
};

/// @brief Represents a lobby room, managing connected players and routing messages through GameBridge.
class GameRoom : public std::enable_shared_from_this<GameRoom> {
    public:
        /// @brief Constructs a new GameRoom instance.
        /// @param id The unique identifier for the room.
        /// @param name The name of the room.
        /// @param password The password for the room, if any.
        /// @param creator_id The ID of the user who created the room.
        /// @param game_config The configuration for the game to be played in the room.
        GameRoom(int id, const std::string& name, const std::string& password, int creator_id, const GameConfig& game_config);

        /// @brief Destructor that ensures the game connection is terminated and resources are cleaned up.
        ~GameRoom();

        /// @brief Gets the unique identifier for the room.
        /// @return The unique identifier for the room.
        int GetId() const;

        /// @brief Gets the name of the room.
        /// @return The name of the room.
        std::string GetName() const;

        /// @brief Gets the name of the game being played in the room.
        /// @return The name of the game being played in the room.
        std::string GetGameName() const;

        /// @brief Checks if the room has a password.
        /// @return true if the room has a password, false otherwise.
        bool HasPassword() const;

        /// @brief Checks if the provided password matches the room's password.
        /// @param pass The password to check.
        /// @return true if the password matches, false otherwise.
        bool CheckPassword(const std::string& pass) const;

        /// @brief Checks if the game is currently running in the room.
        /// @return true if the game is running, false otherwise.
        bool IsPlaying() const;

        /// @brief Checks if the specified client is the creator of the room.
        /// @param client_id The ID of the client to check.
        /// @return true if the client is the creator, false otherwise.
        bool IsCreator(int client_id) const;

        /// @brief Gets the ID of the user who created the room.
        /// @return The ID of the user who created the room.
        int GetCreatorId() const;

        /// @brief Gets the number of players in the room.
        /// @return The number of players in the room.
        size_t GetPlayerCount() const;

        /// @brief Checks if the room has the specified client.
        /// @param client_id The ID of the client to check.
        /// @return true if the room has the client, false otherwise.
        bool HasClient(int client_id);

        /// @brief Adds a client to the room.
        /// @param client_id The ID of the client to add.
        /// @param socket_fd The file descriptor of the client's socket.
        void AddClient(int client_id, int socket_fd);

        /// @brief Removes a client from the room.
        /// @param client_id The ID of the client to remove.
        /// @return true if the room creator changed as a result of the removal, false otherwise.
        bool RemoveClient(int client_id);

        /// @brief Starts the game in the room.
        /// @param allocated_port The port allocated for the game.
        /// @return true if the game started successfully, false otherwise.
        bool StartGame(int allocated_port);

        /// @brief Forwards a command to the game.
        /// @param sender_id The ID of the client who sent the command.
        /// @param effective_id The ID of the client who is the effective user of the command.
        /// @param command The command to forward.
        /// @param msg_id The ID of the message.
        /// @param params The parameters for the command.
        void ForwardCommandToGame(int sender_id, int effective_id, const std::string& command, const std::string& msg_id, const std::string& params);

        /// @brief Broadcasts a message to all clients in the room.
        /// @param message The message to broadcast.
        void BroadcastToRoom(const std::string& message);

        /// @brief Disconnects the game and cleans up resources.
        /// @return true if the game was disconnected successfully, false otherwise.
        bool DisconnectGame();

    private:
        /// @brief The unique identifier for the room.
        int _id;
        /// @brief The name of the room.
        std::string _name;
        /// @brief The password for the room, if any.
        std::string _password;
        /// @brief The ID of the user who created the room.
        std::atomic<int> _creator_id;
        /// @brief The current number of players connected to the room.
        std::atomic<size_t> _player_count;
        /// @brief Indicates whether a game start operation is currently in progress.
        std::atomic<bool> _is_starting;
        /// @brief The GameBridge instance that manages the connection to the game process.
        GameBridge _bridge;

        /// @brief A mapping of client IDs to their corresponding socket file descriptors.
        std::unordered_map<int, int> _clients;
        /// @brief Set of client IDs that were registered in the current match and subsequently left or disconnected.
        std::unordered_set<int> _disconnected_clients;
        /// @brief A mapping of internal message IDs to the pending client request metadata awaiting responses.
        std::unordered_map<std::string, PendingRequest> _pending_responses;
        /// @brief Mutex to protect access to the room's state, ensuring thread safety for client management and game communication.
        std::mutex _room_mutex;

        /// @brief Sends a client registration message (ConnectClient) to the game.
        /// @param client_id The ID of the client to register.
        void SendClientRegistration(int client_id);

        /// @brief Sends a client disconnection message (DisconnectClient) to the game.
        /// @param client_id The ID of the client that left the room.
        void SendClientDisconnection(int client_id);

        /// @brief Sends a client reconnection message (ReconnectClient) to the game.
        /// @param client_id The ID of the client rejoining the active match.
        void SendClientReconnection(int client_id);

        /// @brief Routes a LogChannel message from the game to either all players or a specific IdList.
        /// @param target The target specification ("All" or comma-separated client IDs).
        /// @param raw_line The full newline-delimited message to deliver.
        void RouteLogChannel(const std::string& target, const std::string& raw_line);

        /// @brief Listens for messages from the game and forwards them to the appropriate clients.
        void ListenToGame();
};