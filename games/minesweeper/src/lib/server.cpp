#include "server.hpp"

Server::Server(int port) : _port(port), _server_fd(-1), _is_running(false) {}

Server::~Server() {
    _is_running.store(false);
    if (_server_fd != -1) {
        close(_server_fd);
    }
}

void Server::OnMessageReceived(MessageEventHandler callback) {
    _on_message_callback = callback;
}

void Server::OnClientDisconnected(ClientDisconnectedHandler callback) {
    _on_disconnect_callback = callback;
}

void Server::SendMessage(int client_fd, const std::string& message) {
    if (client_fd == -1 || message.empty()) {
        return;
    }

    const char* data = message.c_str();
    size_t total_sent = 0;
    size_t length = message.length();

    while (total_sent < length) {
        ssize_t sent = send(client_fd, data + total_sent, length - total_sent, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (sent == 0) {
            break;
        }
        total_sent += static_cast<size_t>(sent);
    }
}

void Server::Broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(_clients_mutex);
    for (int fd : _active_clients) {
        SendMessage(fd, message);
    }
}

void Server::Start() {
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[Server] Error binding to port " << _port << "\n";
        return;
    }

    listen(_server_fd, 5);
    _is_running.store(true);
    std::cout << "[Server] Running on port " << _port << "...\n";

    while (_is_running.load()) {
        int client_fd = accept(_server_fd, nullptr, nullptr);
        if (client_fd > 0) {
            {
                std::lock_guard<std::mutex> lock(_clients_mutex);
                _active_clients.push_back(client_fd);
            }
            
            std::thread(&Server::HandleClient, this, client_fd).detach();
        }
    }
}

std::string Server::ReadToken(int socket_fd, std::string& buffer) {
    while (true) {
        size_t pos;
        while ((pos = buffer.find('\n')) == std::string::npos) {
            if (buffer.size() >= MAX_BUFFER_SIZE) {
                buffer.clear();
                return "";
            }

            char temp[256];
            ssize_t bytes_read = read(socket_fd, temp, sizeof(temp));
            if (bytes_read < 0) {
                if (errno == EINTR) {
                    continue;
                }
                buffer.clear();
                return "";
            }
            if (bytes_read == 0) {
                if (buffer.empty()) {
                    return "";
                }
                std::string ultimo = buffer;
                buffer.clear();
                if (!ultimo.empty() && ultimo.back() == '\r') {
                    ultimo.pop_back();
                }
                return ultimo;
            }

            buffer.append(temp, static_cast<size_t>(bytes_read));
        }

        std::string token = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);

        if (!token.empty() && token.back() == '\r') {
            token.pop_back();
        }

        if (!token.empty()) {
            return token;
        }
    }
}

void Server::HandleClient(int client_fd) {
    std::string buffer = "";
            
    while (true) {
        std::string token = ReadToken(client_fd, buffer);
        
        if (token.empty()) {
            break; 
        }

        if (_on_message_callback) {
            _on_message_callback(client_fd, token);
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(_clients_mutex);
        _active_clients.erase(std::remove(_active_clients.begin(), _active_clients.end(), client_fd), _active_clients.end());
    }

    if (_on_disconnect_callback) {
        _on_disconnect_callback(client_fd);
    }

    close(client_fd);
}