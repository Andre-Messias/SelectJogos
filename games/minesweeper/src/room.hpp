#pragma once

#include <string>
#include <chrono>
#include "board.hpp"

enum class RoomState
{
    LOBBY,
    PLAYING,
    NAMING
};

class Room
{
public:
    Room(int id, int size, int bombs, int max_players, const std::string &stats_file);

    void Reset();
    void Start();

    /// @brief Marks the start time of the match when the first click happens.
    void RecordFirstClick();

    /// @brief Transitions to NAMING state and finalizes the time.
    void SetWon(int elapsed_seconds);

    void AddPenalty(int seconds);

    int GetId() const { return _id; }
    int GetMaxPlayers() const { return _max_players; }
    RoomState GetState() const { return _state; }
    const std::string &GetStatsFile() const { return _stats_file; }
    int GetPenaltySeconds() const { return _penalty_seconds; }
    std::chrono::steady_clock::time_point GetStartTime() const { return _start_time; }

    Board &GetBoard() { return _board; }

private:
    int _id;
    int _size;
    int _bombs;
    int _max_players;
    Board _board;
    std::string _stats_file;
    std::chrono::steady_clock::time_point _start_time;
    int _penalty_seconds;
    RoomState _state;
};
