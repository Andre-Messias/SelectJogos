#include "server.hpp"

Server::Server(int port) : _port(port), _server_fd(-1), _is_running(false) {}

Server::~Server() {
    if (_server_fd != -1) {
        close(_server_fd);
    }
}

void Server::OnMessageReceived(MessageEventHandler callback) {
    _on_message_callback = callback;
}

void Server::SendMessage(int client_fd, const std::string& message) {
    write(client_fd, message.c_str(), message.length());
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
        std::cerr << "Error binding to port " << _port << "\n";
        return;
    }

    listen(_server_fd, 5);
    _is_running = true;
    std::cout << "[Server] Running on port " << _port << "...\n";

    while (_is_running) {
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
    size_t pos;
    while ((pos = buffer.find('\n')) == std::string::npos) {
        char temp[256];
        int bytes_read = read(socket_fd, temp, sizeof(temp) - 1);
        if (bytes_read <= 0) {
            if (buffer.empty()) return "";
            std::string ultimo = buffer;
            buffer.clear();
            return ultimo;
        }
        temp[bytes_read] = '\0';
        buffer += temp;
    }
    std::string token = buffer.substr(0, pos);
    buffer.erase(0, pos + 1);
    return token;
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
    close(client_fd);
}
