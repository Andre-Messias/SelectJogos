#pragma once

#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>

/// @brief Manages the TCP connection to the Lobby server and runs the asynchronous receive loop.
class NetworkClient {
    public:
        /// @brief Callback type invoked whenever a complete line is received from the Lobby.
        using MessageCallback = std::function<void(const std::string& line)>;
        /// @brief Callback type invoked when the server closes the connection.
        using DisconnectCallback = std::function<void()>;

        /// @brief Constructs a new NetworkClient instance.
        NetworkClient();

        /// @brief Destructor that ensures the socket is closed and the reader thread is joined.
        ~NetworkClient();

        /// @brief Registers the callback for incoming server lines.
        /// @param cb The function to call on each received line.
        void SetMessageCallback(MessageCallback cb);

        /// @brief Registers the callback for connection loss.
        /// @param cb The function to call if the server disconnects.
        void SetDisconnectCallback(DisconnectCallback cb);

        /// @brief Connects to the Lobby TCP server and starts the background listener thread.
        /// @param host The IPv4 address of the Lobby server.
        /// @param port The TCP port of the Lobby server.
        /// @return true if the connection succeeded, false otherwise.
        bool Connect(const std::string& host, int port);

        /// @brief Closes the TCP connection and stops the background listener thread.
        void Disconnect();

        /// @brief Checks if the TCP connection is currently active.
        /// @return true if connected, false otherwise.
        bool IsConnected() const;

        /// @brief Sends a raw newline-terminated string to the Lobby server.
        /// @param payload The string payload to transmit.
        /// @return true if all bytes were sent, false on socket error.
        bool Send(const std::string& payload);

    private:
        /// @brief Maximum allowed bytes in the receive buffer before dropping the connection.
        static constexpr size_t MAX_BUFFER_SIZE = 65536;

        /// @brief The socket file descriptor connected to the Lobby (-1 if disconnected).
        std::atomic<int> _socket_fd;
        /// @brief Flag indicating whether the client connection is active.
        std::atomic<bool> _is_connected;
        /// @brief Background thread reading incoming messages from the Lobby.
        std::thread _reader_thread;
        /// @brief Mutex serializing outbound socket writes.
        std::mutex _send_mutex;

        /// @brief Callback invoked when a complete line arrives.
        MessageCallback _on_message;
        /// @brief Callback invoked when the connection drops.
        DisconnectCallback _on_disconnect;

        /// @brief Background loop that continuously reads tokens from the socket.
        void ListenLoop();

        /// @brief Reads a single newline-delimited token from the socket stream.
        /// @param fd The socket file descriptor.
        /// @param buffer Persistent stream buffer across reads.
        /// @return The sanitized line without trailing '\r' or '\n', or empty string on EOF/error.
        std::string ReadToken(int fd, std::string& buffer);
};