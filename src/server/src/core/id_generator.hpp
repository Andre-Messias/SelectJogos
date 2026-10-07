#pragma once

#include <unordered_set>
#include <random>
#include <mutex>

class IdGenerator
{
public:
    IdGenerator();

    int AcquireId();

    void ReleaseId(int id);

private:
    static constexpr int MIN_ID = 1000;
    static constexpr int MAX_ID = 999999;
    static constexpr size_t MAX_CAPACITY = static_cast<size_t>(MAX_ID - MIN_ID + 1);

    std::mt19937 _rng;
    std::unordered_set<int> _active_ids;
    std::mutex _id_mutex;
};