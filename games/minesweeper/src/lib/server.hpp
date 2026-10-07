#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

class Server {
    public:
        using MessageEventHandler = std::function<void(int client_fd, const std::string& token)>;

        using ClientDisconnectedHandler = std::function<void(int client_fd)>;

        Server(int port);

        ~Server();

        void OnMessageReceived(MessageEventHandler callback);

        void OnClientDisconnected(ClientDisconnectedHandler callback);

        void SendMessage(int client_fd, const std::string& message);

        void Broadcast(const std::string& message);

        void Start();

    private:
        static constexpr size_t MAX_BUFFER_SIZE = 65536;

        int _port;

        int _server_fd;

        std::atomic<bool> _is_running;
        
        MessageEventHandler _on_message_callback;

        ClientDisconnectedHandler _on_disconnect_callback;

        std::vector<int> _active_clients;

        std::mutex _clients_mutex;

        std::string ReadToken(int socket_fd, std::string& buffer);

        void HandleClient(int client_fd);
};