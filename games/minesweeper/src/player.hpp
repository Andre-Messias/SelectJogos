#pragma once

#include <chrono>
#include <string>

class Player
{
private:
    int _id;
    int _socket_fd;
    bool _connected;
    int _room_id;
    std::chrono::steady_clock::time_point _last_action_time;
    std::string _name;

public:
    Player(int id, int socket_fd);

    int getId() const;
    int getSocketFd() const;
    void setSocketFd(int socket_fd);

    const std::string &getName() const;

    void setName(const std::string &name);

    void resetName();

    void updateActivity();

    bool isInactive(int timeout_seconds) const;

    int getRoomId() const;

    void setRoomId(int room_id);

    bool isConnected() const;

    void setConnected(bool connected);
};