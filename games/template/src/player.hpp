#pragma once

#include <iostream>

/// @brief Represents a player in the game, storing their identity, connection status, and round state.
class Player {
    private:
        /// @brief The unique client ID assigned by the Lobby.
        int _id;
        /// @brief The number chosen by the player in the current round.
        int _number;
        /// @brief Indicates whether the player is currently connected to the room.
        bool _connected;
        /// @brief Indicates whether the player has already submitted an action in the current round.
        bool _has_played;

    public:
        /// @brief Constructs a new Player instance.
        /// @param id The unique client ID of the player.
        /// @param number The initial number for the player (defaults to 0).
        Player(int id, int number = 0);

        /// @brief Gets the unique client ID of the player.
        /// @return The player's client ID.
        int getId() const;

        /// @brief Gets the number submitted by the player.
        /// @return The player's chosen number.
        int getNumber() const;

        /// @brief Sets the number submitted by the player.
        /// @param number The chosen number.
        void setNumber(int number);

        /// @brief Checks if the player is currently connected to the match.
        /// @return true if connected, false if disconnected.
        bool isConnected() const;

        /// @brief Updates the connection status of the player.
        /// @param connected The new connection state.
        void setConnected(bool connected);

        /// @brief Checks if the player has already played in the current round.
        /// @return true if the player already submitted an action, false otherwise.
        bool hasPlayed() const;

        /// @brief Sets whether the player has played in the current round.
        /// @param played The new played state.
        void setHasPlayed(bool played);

        /// @brief Resets the player's round state (number and played flag) for a new round.
        void resetRound();
};