#pragma once

#include <iostream>

class Player {
    private:
        int _id;
        int _number;
        bool _connected;
        bool _has_played;

    public:
        Player(int id, int number = 0);

        int getId() const;

        int getNumber() const;

        void setNumber(int number);

        bool isConnected() const;

        void setConnected(bool connected);

        bool hasPlayed() const;

        void setHasPlayed(bool played);

        void resetRound();
};