#include "player.hpp"

#include <algorithm>
#include <utility>

Player::Player(int client_id, std::string player_name, int size)
    : id(client_id), name(std::move(player_name)), board(size) {}

bool Player::HasPlacedShips() const
{
    return std::any_of(placed.begin(), placed.end(), [](int count)
                       { return count > 0; });
}

bool Player::HasCompleteFleet(const FleetMode &mode) const
{
    return placed == mode.ships;
}

bool Player::CanPlaceShip(int type, const FleetMode &mode) const
{
    return type >= 0 && type < static_cast<int>(placed.size()) &&
           placed[type] < mode.ships[type];
}

bool Player::PlaceShip(int type, int x, int y, bool vertical)
{
    if (!board.Place(type, x, y, vertical))
    {
        return false;
    }

    // Só contabiliza o navio após o tabuleiro aceitar sua posição.
    ++placed[type];
    return true;
}

std::string Player::RemainingShips(const FleetMode &mode) const
{
    std::string remaining = "Faltam: ";
    for (size_t type = 0; type < placed.size(); ++type)
    {
        if (mode.ships[type] > placed[type])
        {
            remaining += std::string(SHIP_NAMES[type]) + "=" +
                         std::to_string(mode.ships[type] - placed[type]) + " ";
        }
    }
    return remaining;
}

void Player::ResetBoard(int size)
{
    board = Board(size);
    page = 1;
}

void Player::BeginBattleView()
{
    own_view = false;
}

void Player::SetView(bool show_own, int selected_page)
{
    own_view = show_own;
    page = selected_page;
}

int Player::VisiblePage(int max_page) const
{
    return std::max(1, std::min(page, max_page));
}
