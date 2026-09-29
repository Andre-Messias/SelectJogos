#include "network_client.hpp"

#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

NetworkClient::NetworkClient()
    : _socket_fd(-1), _is_connected(false) {}

NetworkClient::~NetworkClient() {
    Disconnect();
}

void NetworkClient::SetMessageCallback(MessageCallback cb) {
    _on_message = std::move(cb);
}

void NetworkClient::SetDisconnectCallback(DisconnectCallback cb) {
    _on_disconnect = std::move(cb);
}

bool NetworkClient::Connect(const std::string& host, int port) {
    if (_is_connected.load()) {
        return false;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        close(fd);
        return false;
    }

    if (connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(fd);
        return false;
    }

    _socket_fd.store(fd);
    _is_connected.store(true);

    _reader_thread = std::thread(&NetworkClient::ListenLoop, this);
    return true;
}

void NetworkClient::Disconnect() {
    _is_connected.store(false);
    int fd = _socket_fd.exchange(-1);
    if (fd != -1) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }

    if (_reader_thread.joinable() && _reader_thread.get_id() != std::this_thread::get_id()) {
        _reader_thread.join();
    }
}

bool NetworkClient::IsConnected() const {
    return _is_connected.load();
}

bool NetworkClient::Send(const std::string& payload) {
    std::lock_guard<std::mutex> lock(_send_mutex);
    int fd = _socket_fd.load();
    if (!_is_connected.load() || fd == -1 || payload.empty()) {
        return false;
    }

    const char* data = payload.c_str();
    size_t total_sent = 0;
    size_t length = payload.length();

    while (total_sent < length) {
        ssize_t sent = send(fd, data + total_sent, length - total_sent, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (sent == 0) {
            return false;
        }
        total_sent += static_cast<size_t>(sent);
    }

    return true;
}

void NetworkClient::ListenLoop() {
    std::string buffer;
    while (_is_connected.load()) {
        int fd = _socket_fd.load();
        if (fd == -1) {
            break;
        }

        std::string token = ReadToken(fd, buffer);
        if (token.empty()) {
            break;
        }

        if (_on_message) {
            _on_message(token);
        }
    }

    bool was_connected = _is_connected.exchange(false);
    if (was_connected && _on_disconnect) {
        _on_disconnect();
    }
}

std::string NetworkClient::ReadToken(int fd, std::string& buffer) {
    while (true) {
        size_t pos;
        while ((pos = buffer.find('\n')) == std::string::npos) {
            if (buffer.size() >= MAX_BUFFER_SIZE) {
                buffer.clear();
                return "";
            }

            char temp[256];
            ssize_t bytes_read = read(fd, temp, sizeof(temp));
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
                std::string last = buffer;
                buffer.clear();
                if (!last.empty() && last.back() == '\r') {
                    last.pop_back();
                }
                return last;
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