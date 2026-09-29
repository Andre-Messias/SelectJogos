#pragma once

#include <iostream>

class Player {
    private:
        uint _id;
        int _number;

    public:
        Player(uint id, int number = 0);
        uint getId() const;
        int getNumber() const;
        void setNumber(int number);
};