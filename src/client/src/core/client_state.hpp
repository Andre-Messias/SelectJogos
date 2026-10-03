#pragma once

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>

/// @brief Represents the logical scope of the client within the Lobby lifecycle.
enum class ClientScope {
    /// @brief Connected to the Lobby, not currently in any room.
    IN_LOBBY,
    /// @brief Inside a room, waiting for the match to start.
    ROOM_WAITING,
    /// @brief Inside a room with an active match in progress.
    ROOM_PLAYING
};

/// @brief Stores metadata about a command sent by the client awaiting a server Response.
struct SentCommand {
    /// @brief The primary command name.
    std::string command;
    /// @brief First parameter of the command.
    std::string arg1;
    /// @brief Second parameter of the command.
    std::string arg2;
};

/// @brief Immutable snapshot of the entire client UI state for lock-free rendering.
struct ScreenSnapshot {
    /// @brief The unique ClientID assigned by the Lobby (-1 if not yet assigned).
    int client_id;
    /// @brief The current RoomID (-1 if not in a room).
    int room_id;
    /// @brief The name of the current room.
    std::string room_name;
    /// @brief The name of the game configured for the current room.
    std::string game_name;
    /// @brief The current logical scope of the client.
    ClientScope scope;
    /// @brief Lines currently displayed in the optional game screen canvas.
    std::vector<std::string> canvas_lines;
    /// @brief Formatted event and message logs displayed in the feed area.
    std::vector<std::string> log_lines;
    /// @brief Active error or alert banner text (empty if no alert is active).
    std::string alert_message;
    /// @brief Current text typed by the user in the bottom input prompt.
    std::string input_buffer;
};

/// @brief Thread-safe model holding session metadata.
class ClientState {
    public:
        /// @brief Constructs a default ClientState in the IN_LOBBY scope.
        ClientState();

        /// @brief Sets the ClientID assigned by the Lobby upon connection.
        /// @param id The assigned ClientID.
        void SetClientId(int id);

        /// @brief Gets the ClientID assigned by the Lobby.
        /// @return The current ClientID, or -1 if unassigned.
        int GetClientId();

        /// @brief Updates the state to reflect entering a room.
        /// @param room_id The ID of the room entered.
        /// @param room_name The name of the room (or empty if unknown on JoinRoom).
        /// @param game_name The name of the game (or empty if unknown on JoinRoom).
        void EnterRoom(int room_id, const std::string& room_name, const std::string& game_name);

        /// @brief Updates the room metadata if discovered via room broadcasts or listings.
        /// @param room_name The name of the room.
        /// @param game_name The name of the game.
        void UpdateRoomMetadata(const std::string& room_name, const std::string& game_name);

        /// @brief Resets room information and returns the client scope to IN_LOBBY.
        void LeaveRoom();

        /// @brief Sets the logical scope of the client (IN_LOBBY, ROOM_WAITING, ROOM_PLAYING).
        /// @param scope The new ClientScope value.
        void SetScope(ClientScope scope);

        /// @brief Gets the current logical scope of the client.
        /// @return The current ClientScope.
        ClientScope GetScope();

        /// @brief Replaces the entire game screen canvas with a new set of lines.
        /// @param lines Vector of strings representing the new screen state.
        void SetCanvasLines(const std::vector<std::string>& lines);

        /// @brief Updates a specific 0-indexed line in the game screen canvas, expanding if needed.
        /// @param line_index The index of the line to update.
        /// @param text The new text for the specified line.
        void SetCanvasLine(size_t line_index, const std::string& text);

        /// @brief Clears and hides the game screen canvas.
        void ClearCanvas();

        /// @brief Appends a formatted line to the event log history.
        /// @param formatted_line The log string to add.
        void AddLog(const std::string& formatted_line);

        /// @brief Sets an important error or warning message in the alert bar.
        /// @param alert The alert message to display.
        void SetAlert(const std::string& alert);

        /// @brief Clears the active message in the alert bar.
        void ClearAlert();

        /// @brief Appends a printable character to the user's input buffer.
        /// @param c The character typed by the user.
        void AppendInputChar(char c);

        /// @brief Removes the last character from the user's input buffer (handles UTF-8 continuation bytes).
        void BackspaceInput();

        /// @brief Retrieves and clears the current input buffer when the user presses Enter.
        /// @return The complete command string typed by the user.
        std::string ExtractInput();

        /// @brief Records a sent command so its asynchronous Response can be correlated later.
        /// @param msg_id The generated message ID token.
        /// @param cmd The SentCommand metadata.
        void RegisterSentCommand(const std::string& msg_id, const SentCommand& cmd);

        /// @brief Retrieves and removes a previously registered command by its message ID.
        /// @param msg_id The message ID received in the server Response.
        /// @param out_cmd Reference to store the retrieved SentCommand.
        /// @return true if the msg_id was found, false otherwise.
        bool PopSentCommand(const std::string& msg_id, SentCommand& out_cmd);

        /// @brief Captures a thread-safe snapshot of all state fields for terminal rendering.
        /// @return A ScreenSnapshot copy of the current state.
        ScreenSnapshot GetSnapshot();

    private:
        /// @brief Maximum number of log lines kept in memory.
        static constexpr size_t MAX_LOG_HISTORY = 200;
        /// @brief Maximum allowed lines in the custom game canvas.
        static constexpr size_t MAX_CANVAS_LINES = 100;

        /// @brief The unique ClientID assigned by the Lobby.
        int _client_id;
        /// @brief The ID of the room the client is currently in (-1 if in Lobby).
        int _room_id;
        /// @brief The name of the room the client is currently in.
        std::string _room_name;
        /// @brief The name of the game associated with the current room.
        std::string _game_name;
        /// @brief The current logical state of the client.
        ClientScope _scope;

        /// @brief Lines representing the persistent game screen state.
        std::vector<std::string> _canvas_lines;
        /// @brief Rolling history of event and message logs.
        std::deque<std::string> _log_lines;
        /// @brief Active alert or error message shown above the input bar.
        std::string _alert_message;
        /// @brief Buffer holding the characters currently being typed by the user.
        std::string _input_buffer;

        /// @brief Maps generated MsgIDs to their originating command metadata.
        std::unordered_map<std::string, SentCommand> _sent_commands;
        /// @brief Mutex protecting all state fields for thread-safe access between network and UI threads.
        std::mutex _state_mutex;
};