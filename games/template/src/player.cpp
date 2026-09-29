#include "player.hpp"

Player::Player(uint id, int number) : _id(id), _number(number) {}

uint Player::getId() const {
    return _id;
}

int Player::getNumber() const {
    return _number;
}

void Player::setNumber(int number) {
    _number = number;
}