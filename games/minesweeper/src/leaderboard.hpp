#pragma once

#include <string>
#include <vector>

class Leaderboard
{
public:
    static void SaveTeamScore(const std::string &filename, const std::vector<std::string> &player_names, int total_time, int player_count, const std::string &team_name);

    static std::string GetRankingString(const std::string &diff, const std::string &filename);
};
