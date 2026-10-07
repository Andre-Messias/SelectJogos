#pragma once

#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>

class NetworkClient
{
public:
    using MessageCallback = std::function<void(const std::string &line)>;
    using DisconnectCallback = std::function<void()>;

    NetworkClient();

    ~NetworkClient();

    void SetMessageCallback(MessageCallback cb);

    void SetDisconnectCallback(DisconnectCallback cb);

    bool Connect(const std::string &host, int port);

    void Disconnect();

    bool IsConnected() const;

    bool Send(const std::string &payload);

private:
    static constexpr size_t MAX_BUFFER_SIZE = 65536;

    std::atomic<int> _socket_fd;
    std::atomic<bool> _is_connected;
    std::thread _reader_thread;
    std::mutex _send_mutex;

    MessageCallback _on_message;
    DisconnectCallback _on_disconnect;

    void ListenLoop();

    std::string ReadToken(int fd, std::string &buffer);
};