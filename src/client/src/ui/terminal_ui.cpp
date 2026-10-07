#include "terminal_ui.hpp"
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <cerrno>

namespace
{
    std::string RepeatUTF8(int count, const std::string &token)
    {
        if (count <= 0)
        {
            return "";
        }
        std::string out;
        out.reserve(static_cast<size_t>(count) * token.size());
        for (int i = 0; i < count; ++i)
        {
            out += token;
        }
        return out;
    }

    size_t VisibleLength(const std::string &str)
    {
        size_t len = 0;
        bool in_ansi = false;
        for (size_t i = 0; i < str.length(); ++i)
        {
            unsigned char c = str[i];
            if (c == '\033')
            {
                in_ansi = true;
            }
            else if (in_ansi && c == 'm')
            {
                in_ansi = false;
            }
            else if (!in_ansi)
            {
                // Conta apenas caracteres que não são de continuação UTF-8 (10xxxxxx)
                if ((c & 0xC0) != 0x80)
                {
                    len++;
                }
            }
        }
        return len;
    }
}

TerminalUI::TerminalUI(ClientState &state)
    : _state(state), _is_running(false)
{
    std::memset(&_orig_termios, 0, sizeof(_orig_termios));
}

TerminalUI::~TerminalUI()
{
    DisableRawMode();
}

void TerminalUI::SetCommandCallback(CommandSubmitCallback cb)
{
    _on_submit = std::move(cb);
}

void TerminalUI::EnableRawMode()
{
    tcgetattr(STDIN_FILENO, &_orig_termios);
    termios raw = _orig_termios;
    // Desativa o canonical mode (esperar Enter) e o echo de teclas do terminal
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // Muda para o Buffer de Tela Alternativo (como o vim) e limpa a tela
    std::cout << "\033[?1049h\033[H\033[2J" << std::flush;
}

void TerminalUI::DisableRawMode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &_orig_termios);
    // Restaura o Buffer de Tela Principal
    std::cout << "\033[?1049l" << std::flush;
}

void TerminalUI::RefreshScreen()
{
    Render();
}

void TerminalUI::Run()
{
    EnableRawMode();
    _is_running.store(true);

    Render();

    while (_is_running.load())
    {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }

        if (c == '\n' || c == '\r')
        {
            std::string cmd = _state.ExtractInput();
            if (!cmd.empty())
            {
                if (cmd == "exit" || cmd == "quit")
                {
                    _is_running.store(false);
                    break;
                }
                if (_on_submit)
                {
                    _on_submit(cmd);
                }
            }
        }
        else if (c == 127 || c == '\b')
        { // Apagar
            _state.BackspaceInput();
        }
        else if (c >= 32 && c < 127)
        { // Caracteres ASCII imprimíveis
            _state.AppendInputChar(c);
        }

        Render();
    }

    DisableRawMode();
}

void TerminalUI::Render()
{
    std::lock_guard<std::mutex> lock(_render_mutex);
    ScreenSnapshot snap = _state.GetSnapshot();

    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1)
    {
        w.ws_row = 24;
        w.ws_col = 80;
    }
    int term_rows = (w.ws_row > 0) ? w.ws_row : 24;
    int term_cols = (w.ws_col > 0) ? w.ws_col : 80;

    std::ostringstream ss;

    // 1. Linha de Cabeçalho (Linha 1)
    std::string cid_str = (snap.client_id != -1) ? std::to_string(snap.client_id) : "Connecting...";
    std::string room_info = (snap.room_id != -1)
                                ? ("Room: " + (snap.room_name.empty() ? std::to_string(snap.room_id) : snap.room_name) + " (" + std::to_string(snap.room_id) + ")")
                                : "Lobby";
    std::string scope_str = "[Lobby]";
    if (snap.scope == ClientScope::ROOM_WAITING)
        scope_str = "[Waiting]";
    else if (snap.scope == ClientScope::ROOM_PLAYING)
        scope_str = "[Playing]";

    std::string header_left = " LOBBY CLIENT | ID: " + cid_str + " | " + room_info + " ";
    std::string header_right = " " + scope_str + " ";

    int header_fill = term_cols - static_cast<int>(header_left.length()) - static_cast<int>(header_right.length());
    if (header_fill < 0)
        header_fill = 0;

    ss << "\033[H\033[7m" << header_left << std::string(static_cast<size_t>(header_fill), ' ') << header_right << "\033[0m\r\n";

    // Calcula o espaço vertical disponível
    bool has_canvas = !snap.canvas_lines.empty();
    bool has_alert = !snap.alert_message.empty();

    int canvas_height = has_canvas ? (static_cast<int>(snap.canvas_lines.size()) + 2) : 0;
    int alert_height = has_alert ? 1 : 0;
    int fixed_overhead = 3 + canvas_height + alert_height;
    int log_box_height = term_rows - fixed_overhead;
    if (log_box_height < 3)
        log_box_height = 3;

    // 2. Área do Tabuleiro do Jogo (se ativo)
    if (has_canvas)
    {
        ss << "┌" << RepeatUTF8(term_cols - 2, "─") << "┐\r\n";

        // Encontra a linha mais larga para centralizar o bloco inteiro
        int max_vlen = 0;
        for (const auto &cline : snap.canvas_lines)
        {
            int vlen = static_cast<int>(VisibleLength(cline));
            if (vlen > max_vlen)
                max_vlen = vlen;
        }
        int available = term_cols - 2;
        int global_pad = 0;
        if (max_vlen < available)
        {
            global_pad = (available - max_vlen) / 2;
        }

        for (const auto &cline : snap.canvas_lines)
        {
            ss << "│" << std::string(static_cast<size_t>(global_pad), ' ') << cline << "\033[K\033[" << term_cols << "G│\r\n";
        }
        ss << "└" << RepeatUTF8(term_cols - 2, "─") << "┘\r\n";
    }

    // 3. Área de Log / Feed de Mensagens
    int max_logs = log_box_height;
    int total_logs = static_cast<int>(snap.log_lines.size());
    int start_idx = (total_logs > max_logs) ? (total_logs - max_logs) : 0;

    for (int i = start_idx; i < total_logs; ++i)
    {
        std::string log_line = snap.log_lines[static_cast<size_t>(i)];
        if (static_cast<int>(log_line.length()) > term_cols - 2)
        {
            log_line = log_line.substr(0, static_cast<size_t>(term_cols - 5)) + "...";
        }
        int pad = term_cols - static_cast<int>(log_line.length());
        ss << log_line << (pad > 0 ? std::string(static_cast<size_t>(pad), ' ') : "") << "\r\n";
    }

    // Preenche linhas em branco para não quebrar o layout
    for (int i = total_logs - start_idx; i < max_logs; ++i)
    {
        ss << std::string(static_cast<size_t>(term_cols), ' ') << "\r\n";
    }

    // 4. Barra de Alerta / Erro (se houver)
    if (has_alert)
    {
        std::string alert_text = snap.alert_message;
        if (static_cast<int>(alert_text.length()) > term_cols - 2)
        {
            alert_text = alert_text.substr(0, static_cast<size_t>(term_cols - 5)) + "...";
        }
        int pad = term_cols - static_cast<int>(alert_text.length());
        ss << "\033[1;31m" << alert_text << (pad > 0 ? std::string(static_cast<size_t>(pad), ' ') : "") << "\033[0m\r\n";
    }

    // 5. Barra de Input do Usuário (Base da Tela)
    ss << "├" << RepeatUTF8(term_cols - 2, "─") << "┤\r\n";
    std::string prompt = "> " + snap.input_buffer;
    if (static_cast<int>(prompt.length()) > term_cols - 2)
    {
        prompt = "> " + snap.input_buffer.substr(snap.input_buffer.length() - static_cast<size_t>(term_cols - 5));
    }
    int prompt_pad = term_cols - static_cast<int>(prompt.length());
    ss << prompt << (prompt_pad > 0 ? std::string(static_cast<size_t>(prompt_pad), ' ') : "");

    // Posiciona o cursor logo depois do texto digitado pelo usuário
    int cursor_col = static_cast<int>(prompt.length()) + 1;
    if (cursor_col > term_cols)
        cursor_col = term_cols;
    ss << "\033[" << term_rows << ";" << cursor_col << "H";

    std::cout << ss.str() << std::flush;
}