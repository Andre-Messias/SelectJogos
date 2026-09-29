#include "network_utils.hpp"

#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int NetworkUtils::_next_local_port = NetworkUtils::MIN_LOCAL_PORT;
std::mutex NetworkUtils::_port_mutex;

void NetworkUtils::SendMessage(int socket_fd, const std::string& message) {
    if (socket_fd == -1 || message.empty()) {
        return;
    }

    const char* data = message.c_str();
    size_t total_sent = 0;
    size_t length = message.length();

    while (total_sent < length) {
        ssize_t sent = send(socket_fd, data + total_sent, length - total_sent, MSG_NOSIGNAL);
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

std::string NetworkUtils::ReadToken(int socket_fd, std::string& buffer) {
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

int NetworkUtils::AllocateLocalPort(int reserved_port) {
    std::lock_guard<std::mutex> lock(_port_mutex);

    for (int checked = 0; checked < TOTAL_LOCAL_PORTS; ++checked) {
        int candidate_port = _next_local_port++;
        if (_next_local_port > MAX_LOCAL_PORT) {
            _next_local_port = MIN_LOCAL_PORT;
        }

        if (candidate_port == reserved_port) continue;

        int test_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (test_fd < 0) continue;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(candidate_port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(test_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(test_fd);
            return candidate_port;
        }
        close(test_fd);
    }

    return -1;
}