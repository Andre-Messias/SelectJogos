#pragma once

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// Transporte TCP usado para receber os comandos encaminhados pelo lobby.
class Server {
public:
    using MessageEventHandler = std::function<void(int client_fd, const std::string& token)>;

    explicit Server(int port);
    ~Server();

    // Entrega ao jogo cada comando completo recebido por TCP.
    void OnMessageReceived(MessageEventHandler callback);

    // Envia respostas ao lobby; Broadcast atende todas as conexões ativas.
    void SendMessage(int client_fd, const std::string& message);
    void Broadcast(const std::string& message);

    // Escuta na porta fornecida pelo lobby ao iniciar o processo.
    void Start();

private:
    static constexpr size_t MAX_BUFFER_SIZE = 8192;

    int _port;
    int _server_fd;
    std::atomic<bool> _is_running;
    MessageEventHandler _on_message_callback;
    std::vector<int> _active_clients;
    std::mutex _clients_mutex;

    // TCP entrega bytes; esta função separa os comandos terminados em '\n'.
    std::string ReadToken(int socket_fd, std::string& buffer);
    void HandleClient(int client_fd);
};
