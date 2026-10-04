#pragma once
#include <array>
#include <string>
#include <vector>

enum class Shot { Invalid, Miss, Hit, Sunk };

struct FleetMode {
    const char* name;
    int size;
    std::array<int, 4> ships;
};

extern const std::array<FleetMode, 3> MODES;
extern const std::array<int, 4> SHIP_LENGTHS;
extern const std::array<const char*, 4> SHIP_NAMES;

class Board {
public:
    explicit Board(int size = 10);
    int Size() const { return size_; }
    bool Place(int type, int x, int y, bool vertical);
    Shot Fire(int x, int y);
    bool AllSunk() const;
    std::vector<std::string> Render(bool reveal_ships, int page) const;
private:
    int size_;
    std::vector<std::vector<int>> cells_; // 0 sea; negative ship id; 2 miss; 3 hit
    int next_ship_id_ = 1;
};

bool ParseCoordinate(const std::string& value, int size, int& x, int& y);
