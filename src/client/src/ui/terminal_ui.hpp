#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <functional>
#include <termios.h>
#include "client_state.hpp"

/// @brief Manages terminal raw mode, alternate screen buffer, and atomic TUI rendering.
class TerminalUI {
    public:
        /// @brief Callback type executed when the user presses Enter with an input command.
        using CommandSubmitCallback = std::function<void(const std::string& cmd)>;

        /// @brief Constructs the TerminalUI instance.
        /// @param state Reference to the shared ClientState model.
        explicit TerminalUI(ClientState& state);

        /// @brief Destructor that restores original terminal attributes and leaves alternate screen.
        ~TerminalUI();

        /// @brief Sets the callback invoked when the user submits a command.
        /// @param cb The callback function.
        void SetCommandCallback(CommandSubmitCallback cb);

        /// @brief Starts the terminal UI rendering and keyboard input loop.
        void Run();

        /// @brief Requests an immediate thread-safe redraw of the terminal screen.
        void RefreshScreen();

    private:
        /// @brief Reference to the client state.
        ClientState& _state;
        /// @brief Callback for submitted commands.
        CommandSubmitCallback _on_submit;
        /// @brief Flag controlling the main UI event loop.
        std::atomic<bool> _is_running;
        /// @brief Mutex to serialize concurrent screen renders between network and input threads.
        std::mutex _render_mutex;
        /// @brief Saved original terminal attributes for restoration on exit.
        termios _orig_termios;

        /// @brief Puts the terminal into raw non-canonical mode and switches to alternate buffer.
        void EnableRawMode();

        /// @brief Draws the entire TUI layout atomically to stdout.
        void Render();
    public:
        /// @brief Restores original terminal attributes and switches back to main buffer.
        void DisableRawMode();
};