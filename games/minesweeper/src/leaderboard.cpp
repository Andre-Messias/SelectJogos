#include "leaderboard.hpp"
#include <fstream>
#include <map>
#include <algorithm>
#include <tuple>
#include <cctype>
#include <cstdio>

static std::string EncodeName(const std::string &name)
{
    std::string out = name;
    std::replace(out.begin(), out.end(), ' ', '_');
    return out;
}

static std::string DecodeName(const std::string &token)
{
    bool all_digits = !token.empty() && std::all_of(token.begin(), token.end(), [](unsigned char c)
                                                    { return std::isdigit(c); });
    if (all_digits)
        return "Player " + token;

    const std::string prefix = "Player_";
    if (token.rfind(prefix, 0) == 0 && token.size() > prefix.size() &&
        std::all_of(token.begin() + prefix.size(), token.end(), [](unsigned char c)
                    { return std::isdigit(c); }))
        return "Player " + token.substr(prefix.size());

    return token;
}

void Leaderboard::SaveTeamScore(const std::string &filename, const std::vector<std::string> &player_names, int total_time, int player_count, const std::string &team_name)
{
    std::ofstream outfile(filename, std::ios_base::app);
    if (outfile.is_open())
    {
        for (const auto &name : player_names)
        {
            outfile << EncodeName(name) << " " << total_time << " " << player_count << " " << team_name << "\n";
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

    // Guarda apenas o melhor tempo de cada jogador
    std::map<std::string, std::tuple<int, int, std::string>> best_times;
    std::string p_token;
    int time, p_count;
    std::string t_name;

    while (infile >> p_token >> time >> p_count >> t_name)
    {
        std::string player = DecodeName(p_token);
        if (best_times.find(player) == best_times.end() || time < std::get<0>(best_times[player]))
        {
            best_times[player] = {time, p_count, t_name};
        }
    }

    using Entry = std::pair<std::string, std::tuple<int, int, std::string>>;
    std::vector<Entry> sorted_entries(best_times.begin(), best_times.end());

    std::sort(sorted_entries.begin(), sorted_entries.end(), [](const Entry &a, const Entry &b)
              { return std::get<0>(a.second) < std::get<0>(b.second); });

    std::string out = "\n--- RANKING " + diff + " ---\n";

    int rank = 1;
    for (const auto &entry : sorted_entries)
    {
        const std::string &player = entry.first;
        int t = std::get<0>(entry.second);
        int count = std::get<1>(entry.second);
        const std::string &team = std::get<2>(entry.second);

        int mins = t / 60;
        int secs = t % 60;

        char buf[256];
        snprintf(buf, sizeof(buf), "%2dº | %-16s | %02d:%02d | %-15s | %d jog.\n",
                 rank, player.c_str(), mins, secs, team.c_str(), count);
        out += buf;
        rank++;
    }

    return out;
}
