#pragma once

#include <chrono>
#include <string>

/// @brief Represents a player in the game, storing their identity, connection status, and round state.
class Player
{
private:
    /// @brief The unique client ID assigned by the Lobby.
    int _id;
    /// @brief The socket file descriptor for this player's connection.
    int _socket_fd;
    /// @brief Indicates whether the player is currently connected to the room.
    bool _connected;
    /// @brief Indicates what room the player is currently connected to. (defaults lobby to -1)
    int _room_id;
    /// @brief Stores the time of the player's last action.
    std::chrono::steady_clock::time_point _last_action_time;
    /// @brief Display name shown in the UI and leaderboard (defaults to "Player <id>").
    std::string _name;

public:
    /// @brief Constructs a new Player instance.
    /// @param id The unique client ID of the player.
    /// @param socket_fd The socket file descriptor of the player.
    Player(int id, int socket_fd);

    int getId() const;
    int getSocketFd() const;
    void setSocketFd(int socket_fd);

    /// @brief Gets the player's display name.
    const std::string &getName() const;

    /// @brief Sets the player's display name. Empty names are ignored.
    void setName(const std::string &name);

    /// @brief Resets the player's name to the default value.
    void resetName();

    /// @brief Updates the player's last action time to now.
    void updateActivity();

    /// @brief Checks if the player has been inactive for longer than the timeout.
    bool isInactive(int timeout_seconds) const;

    /// @brief Gets the room ID the player is currently sitting at.
    /// @return The player's current Room ID.
    int getRoomId() const;

    /// @brief Sets the room ID to whatever the command specifies.
    /// @param The player's new Room ID.
    void setRoomId(int room_id);

    /// @brief Checks if the player is currently connected to the match.
    /// @return true if connected, false if disconnected.
    bool isConnected() const;

    /// @brief Updates the connection status of the player.
    /// @param connected The new connection state.
    void setConnected(bool connected);
};