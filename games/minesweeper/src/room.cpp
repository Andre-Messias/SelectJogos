#include "room.hpp"

Room::Room(int id, int size, int bombs, int max_players, const std::string &stats_file)
    : _id(id), _size(size), _bombs(bombs), _max_players(max_players),
      _board(size, bombs), _stats_file(stats_file), _penalty_seconds(0), _state(RoomState::LOBBY)
{
}

void Room::Reset()
{
    _board = Board(_size, _bombs);
    _penalty_seconds = 0;
    _state = RoomState::LOBBY;
}

void Room::Start()
{
    _state = RoomState::PLAYING;
}

void Room::RecordFirstClick()
{
    _start_time = std::chrono::steady_clock::now();
}

void Room::SetWon(int elapsed_seconds)
{
    _penalty_seconds += elapsed_seconds;
    _state = RoomState::NAMING;
}

void Room::AddPenalty(int seconds)
{
    _penalty_seconds += seconds;
}
