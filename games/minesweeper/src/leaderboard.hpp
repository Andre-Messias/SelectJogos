#pragma once

#include <string>
#include <vector>

class Leaderboard
{
public:
    /// @brief Saves a new record for a team that won the game.
    /// @param filename The stats file to write to.
    /// @param player_names Display names of all players in the room (spaces are stored as '_').
    /// @param total_time The final time (including penalties).
    /// @param player_count Number of players.
    /// @param team_name The name chosen by the host.
    static void SaveTeamScore(const std::string &filename, const std::vector<std::string> &player_names, int total_time, int player_count, const std::string &team_name);

    /// @brief Reads the stats file and returns a formatted leaderboard string.
    /// @param diff The difficulty name (easy, medium, hard).
    /// @param filename The stats file to read from.
    /// @return Formatted ranking string.
    static std::string GetRankingString(const std::string &diff, const std::string &filename);
};
