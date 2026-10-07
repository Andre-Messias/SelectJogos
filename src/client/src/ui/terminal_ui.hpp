#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <functional>
#include <termios.h>
#include "client_state.hpp"

class TerminalUI
{
public:
    using CommandSubmitCallback = std::function<void(const std::string &cmd)>;

    explicit TerminalUI(ClientState &state);

    ~TerminalUI();

    void SetCommandCallback(CommandSubmitCallback cb);

    void Run();

    void RefreshScreen();

private:
    ClientState &_state;
    CommandSubmitCallback _on_submit;
    std::atomic<bool> _is_running;
    std::mutex _render_mutex;
    termios _orig_termios;

    void EnableRawMode();

    void Render();

public:
    void DisableRawMode();
};