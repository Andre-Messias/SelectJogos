#include "player.hpp"

Player::Player(int id, int socket_fd)
    : _id(id), _socket_fd(socket_fd), _connected(true), _room_id(-1), _last_action_time(std::chrono::steady_clock::now()),
      _name("Player " + std::to_string(id)) {}

int Player::getId() const { return _id; }
int Player::getSocketFd() const { return _socket_fd; }
void Player::setSocketFd(int socket_fd) { _socket_fd = socket_fd; }

const std::string &Player::getName() const { return _name; }
void Player::setName(const std::string &name)
{
    if (!name.empty())
        _name = name;
}

void Player::resetName()
{
    _name = "Player " + std::to_string(_id);
}

void Player::updateActivity()
{
    _last_action_time = std::chrono::steady_clock::now();
}

bool Player::isInactive(int timeout_seconds) const
{
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - _last_action_time).count();
    return duration >= timeout_seconds;
}

int Player::getRoomId() const
{
    return _room_id;
}

void Player::setRoomId(int room_id)
{
    _room_id = room_id;
}

bool Player::isConnected() const
{
    return _connected;
}

void Player::setConnected(bool connected)
{
    _connected = connected;
}