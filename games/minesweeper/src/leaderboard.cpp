#include "leaderboard.hpp"
#include <fstream>
#include <map>
#include <algorithm>
#include <tuple>
#include <cstdio>

void Leaderboard::SaveTeamScore(const std::string &filename, const std::vector<int> &player_ids, int total_time, int player_count, const std::string &team_name)
{
    std::ofstream outfile(filename, std::ios_base::app);
    if (outfile.is_open())
    {
        for (int id : player_ids)
        {
            outfile << id << " " << total_time << " " << player_count << " " << team_name << "\n";
        }
    }
}

std::string Leaderboard::GetRankingString(const std::string &diff, const std::string &filename)
{
    std::ifstream infile(filename);
    if (!infile.is_open())
    {
        return "Nenhum recorde encontrado ainda para esta dificuldade.\n";
    }

    std::map<int, std::tuple<int, int, std::string>> best_times;
    int p_id, time, p_count;
    std::string t_name;

    while (infile >> p_id >> time >> p_count >> t_name)
    {
        if (best_times.find(p_id) == best_times.end() || time < std::get<0>(best_times[p_id]))
        {
            best_times[p_id] = {time, p_count, t_name};
        }
    }

    using Entry = std::pair<int, std::tuple<int, int, std::string>>;
    std::vector<Entry> sorted_entries(best_times.begin(), best_times.end());

    std::sort(sorted_entries.begin(), sorted_entries.end(), [](const Entry &a, const Entry &b)
              { return std::get<0>(a.second) < std::get<0>(b.second); });

    std::string out = "\n--- RANKING " + diff + " ---\n";

    int rank = 1;
    for (const auto &entry : sorted_entries)
    {
        int player = entry.first;
        int t = std::get<0>(entry.second);
        int count = std::get<1>(entry.second);
        std::string team = std::get<2>(entry.second);

        int mins = t / 60;
        int secs = t % 60;

        char buf[256];
        snprintf(buf, sizeof(buf), "%2dº | Player %-4d | %02d:%02d | %-15s | %d jog.\n",
                 rank, player, mins, secs, team.c_str(), count);
        out += buf;
        rank++;
    }

    return out;
}
