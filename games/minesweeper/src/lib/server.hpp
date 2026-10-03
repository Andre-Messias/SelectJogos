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

/// @brief A simple TCP server class that listens for incoming connections and handles messages from clients.
class Server {
    public:
        /// @brief Type definition for the message event handler callback function.
        using MessageEventHandler = std::function<void(int client_fd, const std::string& token)>;

        /// @brief Type definition for the connection-closed event handler callback function.
        using ClientDisconnectedHandler = std::function<void(int client_fd)>;

        /// @brief Constructor for the Server class.
        /// @param port The port on which the server will listen for incoming connections.
        Server(int port);

        /// @brief Destructor for the Server class. Closes the server socket if it's open.
        ~Server();

        /// @brief Subscribe a callback function to the message received event.
        /// @param callback The function to be called when a message is received. It should accept two parameters: the client file descriptor and the received token.
        void OnMessageReceived(MessageEventHandler callback);

        /// @brief Subscribe a callback to the connection-closed event.
        /// @param callback Called once per connection after its socket stops delivering data, after the fd was removed
        /// from the broadcast list but before it is closed (so the fd number cannot have been reused yet).
        void OnClientDisconnected(ClientDisconnectedHandler callback);

        /// @brief Sends a message to a specific client identified by its socket file descriptor.
        /// @param client_fd The file descriptor of the client's socket.
        /// @param message The message to be sent.
        void SendMessage(int client_fd, const std::string& message);

        /// @brief Sends a message to all currently connected clients.
        /// @param message The message to be sent.
        void Broadcast(const std::string& message);

        /// @brief Start the server to listen for incoming connections and handle messages.
        void Start();

    private:
        /// @brief Maximum allowed bytes in a socket read buffer before dropping the connection.
        static constexpr size_t MAX_BUFFER_SIZE = 65536;

        /// @brief The port number on which the server listens for incoming connections.
        int _port;

        /// @brief The file descriptor for the server socket. Initialized to -1 to indicate that the socket is not yet created.
        int _server_fd;

        /// @brief A flag indicating whether the server is currently running. Used to control the main loop in the Start method.
        std::atomic<bool> _is_running;
        
        /// @brief Handles communication with a connected client. Reads tokens from the client and invokes the message received callback.
        MessageEventHandler _on_message_callback;

        /// @brief Invoked when a client connection closes.
        ClientDisconnectedHandler _on_disconnect_callback;

        /// @brief A list of active client socket file descriptors. Used to keep track of connected clients for broadcasting messages.
        std::vector<int> _active_clients;

        /// @brief A mutex to protect access to the _active_clients vector, ensuring thread-safe operations when adding or removing clients.
        std::mutex _clients_mutex;

        /// @brief Reads a token from the client's socket.
        /// @param socket_fd The file descriptor of the client's socket.
        /// @param buffer The buffer to read data into.
        /// @return The token read from the client.
        std::string ReadToken(int socket_fd, std::string& buffer);

        /// @brief Handles communication with a connected client.
        /// @param client_fd The file descriptor of the client's socket.
        void HandleClient(int client_fd);
};