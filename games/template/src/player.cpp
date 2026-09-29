#include "player.hpp"

Player::Player(int id, int number)
    : _id(id), _number(number), _connected(true), _has_played(false) {}

int Player::getId() const {
    return _id;
}

int Player::getNumber() const {
    return _number;
}

void Player::setNumber(int number) {
    _number = number;
}

bool Player::isConnected() const {
    return _connected;
}

void Player::setConnected(bool connected) {
    _connected = connected;
}

bool Player::hasPlayed() const {
    return _has_played;
}

void Player::setHasPlayed(bool played) {
    _has_played = played;
}

void Player::resetRound() {
    _number = 0;
    _has_played = false;
}