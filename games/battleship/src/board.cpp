#include "board.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

const std::array<FleetMode, 3> MODES{{
    {"rapido", 10, {{4, 3, 0, 0}}},
    {"classico", 15, {{4, 3, 2, 1}}},
    {"longo", 26, {{6, 5, 4, 3}}}
}};

const std::array<int, 4> SHIP_LENGTHS{{1, 2, 4, 5}};
const std::array<const char*, 4> SHIP_NAMES{{
    "submarino", "cruzador", "encouracado", "portaaviao"
}};

Board::Board(int size)
    : size_(size), cells_(size, std::vector<int>(size, 0)) {}

bool Board::Place(int type, int x, int y, bool vertical) {
    if (type < 0 || type >= 4 || x < 0 || y < 0 || x >= size_ || y >= size_) {
        return false;
    }

    // Verifica todo o trajeto e as casas vizinhas antes de alterar o tabuleiro.
    for (int i = 0; i < SHIP_LENGTHS[type]; ++i) {
        int cx = x + (vertical ? 0 : i);
        int cy = y + (vertical ? i : 0);
        if (cx >= size_ || cy >= size_) {
            return false;
        }

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                int nx = cx + dx;
                int ny = cy + dy;
                if (nx >= 0 && ny >= 0 && nx < size_ && ny < size_ &&
                    cells_[ny][nx] != 0) {
                    return false;
                }
            }
        }
    }

    const int id = -next_ship_id_++;
    for (int i = 0; i < SHIP_LENGTHS[type]; ++i) {
        int cx = x + (vertical ? 0 : i);
        int cy = y + (vertical ? i : 0);
        cells_[cy][cx] = id;
    }
    return true;
}

// A casa atingida perde o ID do navio; a busca restante detecta afundamento.
Shot Board::Fire(int x, int y) {
    if (x < 0 || y < 0 || x >= size_ || y >= size_ || cells_[y][x] > 0) {
        return Shot::Invalid;
    }

    int old = cells_[y][x];
    if (old == 0) {
        cells_[y][x] = 2;
        return Shot::Miss;
    }

    cells_[y][x] = 3;
    // Todas as casas de um navio têm o mesmo ID negativo.
    for (const auto& row : cells_) {
        if (std::find(row.begin(), row.end(), old) != row.end()) {
            return Shot::Hit;
        }
    }
    return Shot::Sunk;
}

bool Board::AllSunk() const {
    for (const auto& row : cells_) {
        for (int cell : row) {
            if (cell < 0) {
                return false;
            }
        }
    }
    return true;
}

std::vector<std::string> Board::Render(bool reveal_ships, int page) const {
    std::vector<std::string> lines;
    std::string header = "   ";
    for (int x = 0; x < size_; ++x) {
        header += static_cast<char>('A' + x);
        header += ' ';
    }
    lines.push_back(header);

    // Uma página curta cabe no canvas do cliente até no modo longo.
    const int first = (page - 1) * 10;
    for (int y = first; y < std::min(first + 10, size_); ++y) {
        std::ostringstream line;
        line << (y + 1 < 10 ? " " : "") << y + 1 << ' ';
        for (int x = 0; x < size_; ++x) {
            int cell = cells_[y][x];
            char symbol = '.';
            if (cell == 2) {
                symbol = '~';
            } else if (cell == 3) {
                symbol = 'X';
            } else if (cell < 0 && reveal_ships) {
                symbol = 'O';
            }
            line << symbol << ' ';
        }
        lines.push_back(line.str());
    }
    return lines;
}

bool ParseCoordinate(const std::string& value, int size, int& x, int& y) {
    if (value.size() < 2 || value.size() > 3) {
        return false;
    }

    unsigned char letter = static_cast<unsigned char>(value[0]);
    if (!std::isalpha(letter)) {
        return false;
    }
    x = std::toupper(letter) - 'A';
    if (x < 0 || x >= size) {
        return false;
    }

    int row = 0;
    for (size_t i = 1; i < value.size(); ++i) {
        unsigned char digit = static_cast<unsigned char>(value[i]);
        if (!std::isdigit(digit)) {
            return false;
        }
        row = row * 10 + digit - '0';
    }
    y = row - 1;
    return y >= 0 && y < size;
}
