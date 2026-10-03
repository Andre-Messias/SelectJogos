#pragma once

#include <string>
#include <mutex>

/// @brief Provides low-level socket I/O helpers and dynamic local port allocation.
class NetworkUtils {
    public:
        /// @brief Sends a raw string message to a socket file descriptor, handling partial writes.
        static void SendMessage(int socket_fd, const std::string& message);

        /// @brief Reads a newline-delimited token from a socket stream using a persistent buffer.
        /// @return The sanitized token, or an empty string on EOF, socket error, or buffer overflow.
        static std::string ReadToken(int socket_fd, std::string& buffer);

        /// @brief Thread-safe allocator that finds a free localhost TCP port in [10000, 65534].
        /// @param reserved_port A port to skip (such as the main lobby port).
        /// @return Available port number, or -1 if no ports are free.
        static int AllocateLocalPort(int reserved_port = -1);

    private:
        /// @brief Maximum allowed bytes in a stream buffer before disconnecting an unresponsive/malicious peer.
        static constexpr size_t MAX_BUFFER_SIZE = 65536;

        /// @brief Defines the range of valid local ports for spawning game processes.
        static constexpr int MIN_LOCAL_PORT = 10000;
        /// @brief Defines the maximum valid local port for spawning game processes.
        static constexpr int MAX_LOCAL_PORT = 65534;
        /// @brief Defines the total number of local ports available for spawning game processes.
        static constexpr int TOTAL_LOCAL_PORTS = MAX_LOCAL_PORT - MIN_LOCAL_PORT + 1;

        /// @brief Next available port for spawning local game processes.
        static int _next_local_port;
        /// @brief Mutex to protect access to the local port allocator, ensuring thread safety.
        static std::mutex _port_mutex;
};