#pragma once

#include <string>
#include <mutex>

class NetworkUtils
{
public:
    static void SendMessage(int socket_fd, const std::string &message);

    static std::string ReadToken(int socket_fd, std::string &buffer);

    static int AllocateLocalPort(int reserved_port = -1);

private:
    static constexpr size_t MAX_BUFFER_SIZE = 65536;

    static constexpr int MIN_LOCAL_PORT = 10000;
    static constexpr int MAX_LOCAL_PORT = 65534;
    static constexpr int TOTAL_LOCAL_PORTS = MAX_LOCAL_PORT - MIN_LOCAL_PORT + 1;

    static int _next_local_port;
    static std::mutex _port_mutex;
};