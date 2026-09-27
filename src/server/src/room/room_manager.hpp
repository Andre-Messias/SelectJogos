#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

#include "game_config.hpp"
#include "game_room.hpp"
#include "id_generator.hpp"

/// @brief Manages the game catalog, active rooms lifecycle, and client-to-room associations.
class RoomManager {
    public:
        /// @brief Constructs a RoomManager with the specified game configuration path.
        /// @param config_path The path to the game configuration file.
        explicit RoomManager(const std::string& config_path);

        /// @brief Returns a formatted string listing all configured games.
        std::string FormatGamesList() const;

        /// @brief Returns a formatted string listing all active rooms and their states.
        std::string FormatRoomsList();

        /// @brief Creates a new room and places the creator inside it.
        /// @param creator_id The ID of the client creating the room.
        /// @param creator_fd The socket file descriptor of the creator.
        /// @param room_name The desired name for the new room.
        /// @param game_name The name of the game to be played in the room.
        /// @param password An optional password for the room.
        /// @param out_error A string to hold any error messages if creation fails.
        /// @return The generated room_id, or -1 if creation failed (error written to out_error).
        int CreateRoom(int creator_id, int creator_fd, const std::string& room_name, 
            const std::string& game_name, const std::string& password, std::string& out_error);

        /// @brief Adds a client to an existing room.
        /// @param client_id The ID of the client joining the room.
        /// @param client_fd The socket file descriptor of the client.
        /// @param room_id The ID of the room to join.
        /// @param password The password provided by the client (if any).
        /// @param out_error A string to hold any error messages if joining fails.
        /// @return True if the client successfully joined the room, false otherwise (error written to out_error).
        bool JoinRoom(int client_id, int client_fd, int room_id, const std::string& password, std::string& out_error);

        /// @brief Removes a client from their current room, handling host migration and empty room cleanup.
        /// @param client_id The ID of the client leaving the room.
        /// @return True if the client was successfully removed from a room, false if they were not in any room.
        bool RemoveClientFromRoom(int client_id);

        /// @brief Retrieves the room a client is currently in, or nullptr if in the lobby.
        /// @param client_id The ID of the client.
        /// @return A shared pointer to the GameRoom the client is in, or nullptr if not in any room.
        std::shared_ptr<GameRoom> GetClientRoom(int client_id);

        /// @brief Retrieves the GameConfig for a given game name.
        /// @param game_name The name of the game.
        /// @return A pointer to the GameConfig if found, or nullptr if the game does not exist in the configuration.
        const GameConfig* GetGameConfig(const std::string& game_name) const;

    private:
        /// @brief Maps game names to their corresponding GameConfig objects.
        std::unordered_map<std::string, GameConfig> _available_games;
        /// @brief Maps room IDs to their corresponding GameRoom instances.
        std::unordered_map<int, std::shared_ptr<GameRoom>> _rooms;
        /// @brief Maps client IDs to the room IDs they are currently in.
        std::unordered_map<int, int> _client_to_room;
        /// @brief Generates unique room IDs for new rooms.
        IdGenerator _room_id_gen;
        /// @brief Mutex to protect access to shared resources in the RoomManager, ensuring thread safety.
        std::mutex _manager_mutex;
};